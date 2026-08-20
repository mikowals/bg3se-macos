/*
 * Tier 0: Ext.Json.Stringify over objects that expose __pairs.
 *
 * WHY THIS MATTERS: the extender hands mods component proxies as userdata with
 * a __pairs metamethod -- that is how `for k,v in pairs(entity.Health)` works.
 * The JSON serializer has no userdata case, so every such object serializes to
 * the string "null". Dumping or persisting a component silently yields nothing,
 * with no error raised.
 *
 * Offline: builds the proxy in plain Lua, no game required.
 */

#include "test_harness.h"
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <string.h>

#include "lua_json.h"

/* The user-facing entry: Ext.Json.Stringify. */
extern int lua_ext_json_stringify(lua_State *L);

/* A userdata whose metatable supplies __pairs, i.e. the shape of a component
 * proxy. Iterating it yields Current=7, Max=12. */
static const char *PROXY_LUA =
    "local ud = ... \n"
    "local backing = { Current = 7, Max = 12 }\n"
    "local mt = {}\n"
    "mt.__index = function(_, k) return backing[k] end\n"
    "mt.__pairs = function(self)\n"
    "  return function(_, k) return next(backing, k) end, self, nil\n"
    "end\n"
    "debug.setmetatable(ud, mt)\n"
    "return ud\n";

static void push_proxy(lua_State *L) {
    ASSERT_EQ(luaL_loadstring(L, PROXY_LUA), LUA_OK);
    lua_newuserdata(L, 1);
    ASSERT_EQ(lua_pcall(L, 1, 1, 0), LUA_OK);
}

/* Call Ext.Json.Stringify exactly as a mod would: one argument, one result. */
static void stringify_top(lua_State *L, char *out, size_t n) {
    lua_pushcfunction(L, lua_ext_json_stringify);
    lua_insert(L, -2);                       /* [fn, value] */
    ASSERT_EQ(lua_pcall(L, 1, 1, 0), LUA_OK);
    const char *s = lua_tostring(L, -1);
    snprintf(out, n, "%s", s ? s : "(nil)");
    lua_pop(L, 1);
}

TEST(json_stringify_pairs_proxy_is_not_null) {
    lua_State *L = luaL_newstate();
    ASSERT_NOT_NULL(L);
    luaL_openlibs(L);

    push_proxy(L);
    char json[512];
    stringify_top(L, json, sizeof(json));

    // The defect writes exactly "null" for any userdata.
    ASSERT_TRUE(strcmp(json, "null") != 0);
    lua_close(L);
}

TEST(json_stringify_pairs_proxy_has_fields) {
    lua_State *L = luaL_newstate();
    ASSERT_NOT_NULL(L);
    luaL_openlibs(L);

    push_proxy(L);
    char json[512];
    stringify_top(L, json, sizeof(json));

    ASSERT_NOT_NULL(strstr(json, "Current"));
    ASSERT_NOT_NULL(strstr(json, "7"));
    ASSERT_NOT_NULL(strstr(json, "Max"));
    ASSERT_NOT_NULL(strstr(json, "12"));
    lua_close(L);
}

/* A proxy nested inside a plain table must not poison its siblings. */
TEST(json_stringify_proxy_nested_in_table) {
    lua_State *L = luaL_newstate();
    ASSERT_NOT_NULL(L);
    luaL_openlibs(L);

    lua_newtable(L);
    lua_pushstring(L, "ok");
    lua_setfield(L, -2, "sibling");
    push_proxy(L);
    lua_setfield(L, -2, "proxy");

    char json[512];
    stringify_top(L, json, sizeof(json));

    ASSERT_NOT_NULL(strstr(json, "sibling"));
    ASSERT_NOT_NULL(strstr(json, "Current"));
    lua_close(L);
}

void register_json_proxy_tests(void) {
    printf("[json_proxies]\n");
    RUN_TEST(json_stringify_pairs_proxy_is_not_null);
    RUN_TEST(json_stringify_pairs_proxy_has_fields);
    RUN_TEST(json_stringify_proxy_nested_in_table);
}
