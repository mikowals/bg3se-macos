/*
 * Tier 0: Ext.Json.Stringify over objects that expose __pairs.
 *
 * WHY THIS MATTERS: the extender hands mods component proxies as userdata with
 * a __pairs metamethod -- that is how `for k,v in pairs(entity.Health)` works.
 * The JSON serializer has no userdata case, so every such object serializes to
 * the string "null". Dumping or persisting a component silently yields nothing,
 * with no error raised.
 *
 * The fix serializes __pairs userdata in place inside the serializer, through
 * the same depth cap (JSON_MAX_DEPTH = 200) and cycle guard as tables. These
 * tests pin that contract: plain-table output is unchanged, cycles terminate
 * with a null back-edge, deep trees survive, opaque userdata stays null, and a
 * raising __pairs degrades to null for that node without aborting the call.
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

/* Run a chunk that returns one value, then stringify that value. */
static void stringify_chunk(lua_State *L, const char *chunk, char *out, size_t n) {
    ASSERT_EQ(luaL_loadstring(L, chunk), LUA_OK);
    ASSERT_EQ(lua_pcall(L, 0, 1, 0), LUA_OK);
    stringify_top(L, out, n);
}

/* Golden: plain-table output is byte-identical to the pre-rework serializer.
 * (One string key per object: Lua's string hash is seeded per state, so a
 * multi-key object has no stable field order to pin.) */
TEST(json_stringify_plain_table_golden) {
    lua_State *L = luaL_newstate();
    ASSERT_NOT_NULL(L);
    luaL_openlibs(L);

    char json[512];
    stringify_chunk(L, "return {1, 'two', true, {k = 2.5}, {{}}}", json, sizeof(json));
    ASSERT_STR_EQ(json, "[1,\"two\",true,{\"k\":2.5},[{}]]");
    lua_close(L);
}

/* A self-referential table terminates with a null back-edge. */
TEST(json_stringify_linear_cycle_is_null_backedge) {
    lua_State *L = luaL_newstate();
    ASSERT_NOT_NULL(L);
    luaL_openlibs(L);

    char json[512];
    stringify_chunk(L, "local t = {}; t.self = t; return t", json, sizeof(json));
    ASSERT_STR_EQ(json, "{\"self\":null}");
    lua_close(L);
}

/* t.a = t; t.b = t used to expand exponentially against the depth cap alone.
 * With the cycle guard it is two null back-edges. */
TEST(json_stringify_branching_cycle_terminates) {
    lua_State *L = luaL_newstate();
    ASSERT_NOT_NULL(L);
    luaL_openlibs(L);

    char json[512];
    stringify_chunk(L, "local t = {}; t.a = t; t.b = t; return t", json, sizeof(json));
    ASSERT_TRUE(strlen(json) < 32);
    ASSERT_NOT_NULL(strstr(json, "\"a\":null"));
    ASSERT_NOT_NULL(strstr(json, "\"b\":null"));
    lua_close(L);
}

/* Shared (non-cyclic) references are not back-edges: the same leaf table
 * reached twice via different parents serializes both times. */
TEST(json_stringify_shared_reference_is_not_a_cycle) {
    lua_State *L = luaL_newstate();
    ASSERT_NOT_NULL(L);
    luaL_openlibs(L);

    char json[512];
    stringify_chunk(L, "local leaf = {v = 1}; return {leaf, leaf}", json, sizeof(json));
    ASSERT_STR_EQ(json, "[{\"v\":1},{\"v\":1}]");
    lua_close(L);
}

/* A 40-deep tree survives intact: a materializing rework capped at 32 would
 * silently drop the leaf. */
TEST(json_stringify_depth_40_preserved) {
    lua_State *L = luaL_newstate();
    ASSERT_NOT_NULL(L);
    luaL_openlibs(L);

    char json[2048];
    stringify_chunk(L,
        "local t = {leaf = 1}\n"
        "for _ = 1, 40 do t = {t} end\n"
        "return t", json, sizeof(json));
    ASSERT_NOT_NULL(strstr(json, "\"leaf\":1"));
    ASSERT_NULL(strstr(json, "null"));
    lua_close(L);
}

/* Past JSON_MAX_DEPTH (200) the serializer still emits null, as before. */
TEST(json_stringify_depth_over_cap_truncates_to_null) {
    lua_State *L = luaL_newstate();
    ASSERT_NOT_NULL(L);
    luaL_openlibs(L);

    char json[4096];
    stringify_chunk(L,
        "local t = {leaf = 1}\n"
        "for _ = 1, 210 do t = {t} end\n"
        "return t", json, sizeof(json));
    ASSERT_NOT_NULL(strstr(json, "null"));
    ASSERT_NULL(strstr(json, "leaf"));
    lua_close(L);
}

/* Userdata without __pairs is opaque and stays null. */
TEST(json_stringify_opaque_userdata_is_null) {
    lua_State *L = luaL_newstate();
    ASSERT_NOT_NULL(L);
    luaL_openlibs(L);

    lua_newuserdata(L, 1);
    char json[64];
    stringify_top(L, json, sizeof(json));
    ASSERT_STR_EQ(json, "null");
    lua_close(L);
}

/* A proxy whose field is another proxy nests as an object inside an object. */
TEST(json_stringify_proxy_inside_proxy) {
    lua_State *L = luaL_newstate();
    ASSERT_NOT_NULL(L);
    luaL_openlibs(L);

    char json[512];

    /* Inner proxy, then outer proxy whose backing holds the inner one. */
    push_proxy(L);                                   /* inner: Current=7, Max=12 */
    lua_setglobal(L, "__inner");
    ASSERT_EQ(luaL_loadstring(L,
        "local ud = ...\n"
        "local backing = { Inner = __inner, Name = 'outer' }\n"
        "local mt = {}\n"
        "mt.__pairs = function(self)\n"
        "  return function(_, k) return next(backing, k) end, self, nil\n"
        "end\n"
        "debug.setmetatable(ud, mt)\n"
        "return ud\n"), LUA_OK);
    lua_newuserdata(L, 1);
    ASSERT_EQ(lua_pcall(L, 1, 1, 0), LUA_OK);
    stringify_top(L, json, sizeof(json));

    ASSERT_NOT_NULL(strstr(json, "\"Name\":\"outer\""));
    ASSERT_NOT_NULL(strstr(json, "\"Inner\":{"));
    ASSERT_NOT_NULL(strstr(json, "\"Current\":7"));
    lua_close(L);
}

/* A proxy that yields itself is a back-edge, caught by pointer identity. */
TEST(json_stringify_proxy_cycle_is_null_backedge) {
    lua_State *L = luaL_newstate();
    ASSERT_NOT_NULL(L);
    luaL_openlibs(L);

    ASSERT_EQ(luaL_loadstring(L,
        "local ud = ...\n"
        "local mt = {}\n"
        "mt.__pairs = function(self)\n"
        "  local backing = { Self = self, N = 1 }\n"
        "  return function(_, k) return next(backing, k) end, self, nil\n"
        "end\n"
        "debug.setmetatable(ud, mt)\n"
        "return ud\n"), LUA_OK);
    lua_newuserdata(L, 1);
    ASSERT_EQ(lua_pcall(L, 1, 1, 0), LUA_OK);

    char json[512];
    stringify_top(L, json, sizeof(json));
    ASSERT_NOT_NULL(strstr(json, "\"Self\":null"));
    ASSERT_NOT_NULL(strstr(json, "\"N\":1"));
    lua_close(L);
}

/* A __pairs that raises degrades to null for that node; the sibling still
 * serializes and the call itself succeeds (fail-soft, logged). */
TEST(json_stringify_raising_pairs_is_null_and_does_not_abort) {
    lua_State *L = luaL_newstate();
    ASSERT_NOT_NULL(L);
    luaL_openlibs(L);

    ASSERT_EQ(luaL_loadstring(L,
        "local ud = ...\n"
        "debug.setmetatable(ud, { __pairs = function() error('proxy dead') end })\n"
        "return { bad = ud, sibling = 'ok' }\n"), LUA_OK);
    lua_newuserdata(L, 1);
    ASSERT_EQ(lua_pcall(L, 1, 1, 0), LUA_OK);

    char json[512];
    stringify_top(L, json, sizeof(json));
    ASSERT_NOT_NULL(strstr(json, "\"bad\":null"));
    ASSERT_NOT_NULL(strstr(json, "\"sibling\":\"ok\""));
    lua_close(L);
}

/* An iterator that raises mid-walk discards the partial object for that node. */
TEST(json_stringify_raising_iterator_rewinds_node) {
    lua_State *L = luaL_newstate();
    ASSERT_NOT_NULL(L);
    luaL_openlibs(L);

    ASSERT_EQ(luaL_loadstring(L,
        "local ud = ...\n"
        "local mt = {}\n"
        "mt.__pairs = function(self)\n"
        "  local n = 0\n"
        "  return function()\n"
        "    n = n + 1\n"
        "    if n == 1 then return 'First', 1 end\n"
        "    error('iterator dead')\n"
        "  end, self, nil\n"
        "end\n"
        "debug.setmetatable(ud, mt)\n"
        "return { bad = ud, sibling = 'ok' }\n"), LUA_OK);
    lua_newuserdata(L, 1);
    ASSERT_EQ(lua_pcall(L, 1, 1, 0), LUA_OK);

    char json[512];
    stringify_top(L, json, sizeof(json));
    ASSERT_NOT_NULL(strstr(json, "\"bad\":null"));
    ASSERT_NULL(strstr(json, "First"));
    ASSERT_NOT_NULL(strstr(json, "\"sibling\":\"ok\""));
    lua_close(L);
}

/* The serializer leaves the stack exactly as it found it. */
TEST(json_stringify_proxy_is_stack_balanced) {
    lua_State *L = luaL_newstate();
    ASSERT_NOT_NULL(L);
    luaL_openlibs(L);

    push_proxy(L);
    int top_before = lua_gettop(L);
    luaL_Buffer b;
    luaL_buffinit(L, &b);
    json_stringify_value(L, top_before, &b);
    luaL_pushresult(&b);
    ASSERT_EQ(lua_gettop(L), top_before + 1);
    ASSERT_NOT_NULL(strstr(lua_tostring(L, -1), "Current"));
    lua_close(L);
}

void register_json_proxy_tests(void) {
    printf("[json_proxies]\n");
    RUN_TEST(json_stringify_pairs_proxy_is_not_null);
    RUN_TEST(json_stringify_pairs_proxy_has_fields);
    RUN_TEST(json_stringify_proxy_nested_in_table);
    RUN_TEST(json_stringify_plain_table_golden);
    RUN_TEST(json_stringify_linear_cycle_is_null_backedge);
    RUN_TEST(json_stringify_branching_cycle_terminates);
    RUN_TEST(json_stringify_shared_reference_is_not_a_cycle);
    RUN_TEST(json_stringify_depth_40_preserved);
    RUN_TEST(json_stringify_depth_over_cap_truncates_to_null);
    RUN_TEST(json_stringify_opaque_userdata_is_null);
    RUN_TEST(json_stringify_proxy_inside_proxy);
    RUN_TEST(json_stringify_proxy_cycle_is_null_backedge);
    RUN_TEST(json_stringify_raising_pairs_is_null_and_does_not_abort);
    RUN_TEST(json_stringify_raising_iterator_rewinds_node);
    RUN_TEST(json_stringify_proxy_is_stack_balanced);
}
