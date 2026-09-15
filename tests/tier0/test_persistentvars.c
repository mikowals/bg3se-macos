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
 * The fixture is hermetic: the original HOME is restored and the temp tree is
 * removed with mkdir/unlink/rmdir, no shell involved.
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
#include <sys/stat.h>

#include "lua_persistentvars.h"

static char s_test_home[PATH_MAX];
static char s_saved_home[PATH_MAX];
static int  s_saved_home_set;

/* Setup is all-or-nothing: HOME is captured before anything can fail, the
 * temp tree is built in a local buffer, and s_test_home (the "setup complete"
 * marker) is published only after HOME has been redirected. A failure midway
 * (TMPDIR unwritable, say) therefore leaves the real HOME untouched and no
 * later test can persist into the real BG3SE support directory. */
static void setup_home(void) {
    if (s_test_home[0] != '\0') return;
    if (!s_saved_home_set) {
        const char *home = getenv("HOME");
        if (home) {
            snprintf(s_saved_home, sizeof(s_saved_home), "%s", home);
            s_saved_home_set = 1;
        }
    }
    const char *tmp = getenv("TMPDIR");
    if (!tmp || !tmp[0]) tmp = "/tmp";
    char tree[PATH_MAX];
    snprintf(tree, sizeof(tree), "%s/bg3se_t0_pv_XXXXXX", tmp);
    ASSERT_NOT_NULL(mkdtemp(tree));
    // persist_init() only mkdirs the leaf; pre-create the intermediate dirs
    // that exist in a real $HOME.
    char dir[PATH_MAX];
    snprintf(dir, sizeof(dir), "%s/Library", tree);
    ASSERT_EQ(mkdir(dir, 0700), 0);
    snprintf(dir, sizeof(dir), "%s/Library/Application Support", tree);
    ASSERT_EQ(mkdir(dir, 0700), 0);
    // Must happen before the FIRST persist call: get_support_dir() caches HOME.
    ASSERT_EQ(setenv("HOME", tree, 1), 0);
    snprintf(s_test_home, sizeof(s_test_home), "%s", tree);
}

// Remove the temp HOME tree: the persistentvars file, then each directory
// from the leaf up. Restores the caller's HOME.
static void teardown_home(void) {
    if (s_test_home[0] == '\0') return;
    char path[PATH_MAX];
    snprintf(path, sizeof(path),
             "%s/Library/Application Support/BG3SE/persistentvars/BG3SE_T0.json",
             s_test_home);
    (void)unlink(path);
    static const char *const leaves[] = {
        "/Library/Application Support/BG3SE/persistentvars",
        "/Library/Application Support/BG3SE",
        "/Library/Application Support",
        "/Library",
        "",
    };
    for (size_t i = 0; i < sizeof(leaves) / sizeof(leaves[0]); i++) {
        snprintf(path, sizeof(path), "%s%s", s_test_home, leaves[i]);
        (void)rmdir(path);
    }
    if (s_saved_home_set) setenv("HOME", s_saved_home, 1); else unsetenv("HOME");
    s_test_home[0] = '\0';
}

static char *pv_dir_path(char *buf, size_t n) {
    snprintf(buf, n, "%s/Library/Application Support/BG3SE/persistentvars",
             s_test_home);
    return buf;
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
    ASSERT_TRUE(persist_save_all(L));
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
}

/* The periodic save is entered from native code (the Osiris tick hook) with
 * no Lua frame above it, so anything that raises inside the walk would
 * longjmp through the game. A __pairs callback that deletes the key lua_next
 * is holding and forces a rehash makes the next lua_next raise "invalid key
 * to 'next'": the save must absorb that (protected call), stay stack
 * balanced, report failure, and keep the dirty flag so it retries. */
TEST(persist_save_survives_mutating_pairs_callback) {
    setup_home();
    lua_State *L = luaL_newstate();
    ASSERT_NOT_NULL(L);
    luaL_openlibs(L);

    ASSERT_EQ(luaL_loadstring(L,
        "local u = ...\n"
        "local pv = {}\n"
        "debug.setmetatable(u, { __pairs = function()\n"
        "  pv.bad = nil\n"
        "  for i = 1, 1000 do pv['k' .. i] = i end\n"
        "  return function() return nil end, nil, nil\n"
        "end })\n"
        "pv.bad = u\n"
        "Mods = { BG3SE_T0 = { PersistentVars = pv } }\n"), LUA_OK);
    lua_newuserdata(L, 1);
    ASSERT_EQ(lua_pcall(L, 1, 0, 0), LUA_OK);

    persist_mark_dirty();
    int top_before = lua_gettop(L);
    ASSERT_FALSE(persist_save_all(L));
    ASSERT_EQ(lua_gettop(L), top_before);
    ASSERT_TRUE(persist_is_dirty());
    lua_close(L);
}

/* A mod table with an __index metamethod (Mods.X = setmetatable({}, {...}))
 * must not have mod code run from the save path: the PersistentVars read is
 * raw, so the metamethod is never consulted and the save succeeds. */
TEST(persist_save_does_not_invoke_mod_table_index) {
    setup_home();
    lua_State *L = luaL_newstate();
    ASSERT_NOT_NULL(L);
    luaL_openlibs(L);

    ASSERT_EQ(luaL_dostring(L,
        "Mods = { Bad = setmetatable({}, { __index = function()\n"
        "  error('save explosion')\n"
        "end }) }\n"
        "Mods.BG3SE_T0 = { PersistentVars = { marker = 'raw_ok' } }\n"), LUA_OK);

    int top_before = lua_gettop(L);
    ASSERT_TRUE(persist_save_all(L));
    ASSERT_EQ(lua_gettop(L), top_before);
    ASSERT_FALSE(persist_is_dirty());
    lua_close(L);
}

/* A write that fails (unwritable directory) must not be reported as success
 * and must not clear the dirty flag, or the periodic save never retries. */
TEST(persist_failed_write_keeps_dirty_and_returns_false) {
    setup_home();
    lua_State *L = luaL_newstate();
    ASSERT_NOT_NULL(L);
    luaL_openlibs(L);

    ASSERT_EQ(luaL_dostring(L,
        "Mods = { BG3SE_T0 = { PersistentVars = { marker = 'first' } } }"), LUA_OK);
    ASSERT_TRUE(persist_save_all(L));          // creates the directory

    char dir[PATH_MAX];
    pv_dir_path(dir, sizeof(dir));
    if (geteuid() == 0) { lua_close(L); return; }   // root ignores modes
    ASSERT_EQ(chmod(dir, 0500), 0);
    persist_mark_dirty();
    ASSERT_FALSE(persist_save_all(L));
    ASSERT_TRUE(persist_is_dirty());

    ASSERT_EQ(chmod(dir, 0700), 0);
    ASSERT_TRUE(persist_save_all(L));
    ASSERT_FALSE(persist_is_dirty());
    lua_close(L);
}

void register_persistentvars_tests(void) {
    printf("[persistentvars]\n");
    RUN_TEST(persist_save_writes_live_table);
    RUN_TEST(persist_roundtrip_restores_values);
    RUN_TEST(persist_save_survives_mutating_pairs_callback);
    RUN_TEST(persist_save_does_not_invoke_mod_table_index);
    RUN_TEST(persist_failed_write_keeps_dirty_and_returns_false);
    teardown_home();
}
