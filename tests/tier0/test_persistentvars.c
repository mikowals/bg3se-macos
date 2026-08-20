/*
 * Tier 0 tests: PersistentVars disk roundtrip (offline, real lua_State).
 *
 * Regression for the empty-JSON bug: persist_save_all() computed the stack
 * index of PersistentVars with lua_gettop() AFTER luaL_buffinit(). Lua 5.4's
 * luaL_buffinit pushes a light-userdata placeholder, so the serializer was
 * handed the placeholder (-> "null", 4 bytes) instead of the live table.
 *
 * HOME is redirected to a temp directory before the first persist call so the
 * module writes under <tmp>/Library/Application Support/BG3SE/persistentvars/.
 */

#include "test_harness.h"

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "lua_persistentvars.h"

static char s_test_home[PATH_MAX];

static void setup_home(void) {
    if (s_test_home[0] != '\0') return;
    const char *tmp = getenv("TMPDIR");
    if (!tmp || !tmp[0]) tmp = "/tmp";
    snprintf(s_test_home, sizeof(s_test_home), "%s/bg3se_t0_pv_XXXXXX", tmp);
    ASSERT_NOT_NULL(mkdtemp(s_test_home));
    // persist_init() only mkdirs the leaf; pre-create the intermediate dirs
    // that exist in a real $HOME.
    char cmd[PATH_MAX + 64];
    snprintf(cmd, sizeof(cmd),
             "mkdir -p '%s/Library/Application Support'", s_test_home);
    ASSERT_EQ(system(cmd), 0);
    // Must happen before the FIRST persist call: get_support_dir() caches HOME.
    setenv("HOME", s_test_home, 1);
}

static char *pv_file_path(char *buf, size_t n) {
    snprintf(buf, n,
             "%s/Library/Application Support/BG3SE/persistentvars/BG3SE_T0.json",
             s_test_home);
    return buf;
}

static char *read_all(const char *path, size_t *out_len) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size < 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)size + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t rd = fread(buf, 1, (size_t)size, f);
    fclose(f);
    buf[rd] = '\0';
    if (out_len) *out_len = rd;
    return buf;
}

TEST(persist_save_writes_live_table) {
    setup_home();
    lua_State *L = luaL_newstate();
    ASSERT_NOT_NULL(L);
    luaL_openlibs(L);

    ASSERT_EQ(luaL_dostring(L,
        "Mods = { BG3SE_T0 = { PersistentVars = {"
        "  marker = 'tier0_marker', num = 42,"
        "  nested = { list = {1, 2, 3} }"
        "} } }"), LUA_OK);

    int top_before = lua_gettop(L);
    persist_save_all(L);
    ASSERT_EQ(lua_gettop(L), top_before);  // save must be stack-balanced

    char path[PATH_MAX];
    size_t len = 0;
    char *json = read_all(pv_file_path(path, sizeof(path)), &len);
    ASSERT_NOT_NULL(json);
    // The bug wrote exactly "null" (4 bytes). Real content must be an object
    // containing the marker.
    ASSERT_TRUE(len > 4);
    ASSERT_NE(json[0], 'n');
    ASSERT_NOT_NULL(strstr(json, "tier0_marker"));
    ASSERT_NOT_NULL(strstr(json, "\"num\""));
    free(json);
    lua_close(L);
}

TEST(persist_roundtrip_restores_values) {
    setup_home();
    lua_State *L = luaL_newstate();
    ASSERT_NOT_NULL(L);
    luaL_openlibs(L);

    // Mod-style usage: mutate an existing PersistentVars table.
    ASSERT_EQ(luaL_dostring(L,
        "Mods = { BG3SE_T0 = { PersistentVars = {} } }\n"
        "Mods.BG3SE_T0.PersistentVars.marker = 'tier0_marker'\n"
        "Mods.BG3SE_T0.PersistentVars.num = 42\n"
        "Mods.BG3SE_T0.PersistentVars.nested = { list = {1, 2, 3} }\n"), LUA_OK);
    persist_save_all(L);

    // Clobber in memory (both value overwrite AND table replacement).
    ASSERT_EQ(luaL_dostring(L,
        "Mods.BG3SE_T0.PersistentVars = { marker = 'CLOBBERED' }"), LUA_OK);

    int top_before = lua_gettop(L);
    persist_restore_all(L);
    ASSERT_EQ(lua_gettop(L), top_before);  // restore must be stack-balanced

    ASSERT_EQ(luaL_dostring(L,
        "local pv = Mods.BG3SE_T0.PersistentVars\n"
        "assert(pv.marker == 'tier0_marker', 'marker: ' .. tostring(pv.marker))\n"
        "assert(pv.num == 42, 'num: ' .. tostring(pv.num))\n"
        "assert(pv.nested.list[2] == 2, 'nested list')\n"), LUA_OK);
    lua_close(L);

    // Cleanup the temp HOME tree (best-effort).
    char cmd[PATH_MAX + 16];
    snprintf(cmd, sizeof(cmd), "rm -rf '%s'", s_test_home);
    (void)system(cmd);
}

void register_persistentvars_tests(void) {
    printf("[persistentvars]\n");
    RUN_TEST(persist_save_writes_live_table);
    RUN_TEST(persist_roundtrip_restores_values);
}
