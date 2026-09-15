/*
 * Tier 0 tests for Ext.Enums value lookup (src/enum/enum_ext.c).
 *
 * Pure logic: builds a real Lua state, registers the hardcoded enum
 * definitions and the Ext.Enums table, then exercises the __index
 * metamethod from Lua. No game process, no game memory.
 */

#include "test_harness.h"
#include "enum_registry.h"

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------

static lua_State *make_state(void) {
    lua_State *L = luaL_newstate();
    if (!L) return NULL;
    luaL_openlibs(L);

    enum_registry_init();
    enum_register_definitions();
    enum_register_metatables(L);

    // Build a global `Ext` table and hang Ext.Enums off it.
    lua_newtable(L);              // Ext
    enum_register_ext_enums(L);   // consumes top-of-stack Ext table's slot
    lua_setglobal(L, "Ext");

    return L;
}

// Run a Lua chunk that must `return` a string; returns 1 on success and copies
// the result into out. Returns 0 if the chunk errored or returned a non-string.
static int eval_string(lua_State *L, const char *chunk, char *out, size_t out_sz) {
    int base = lua_gettop(L);
    if (luaL_loadstring(L, chunk) != LUA_OK) {
        lua_settop(L, base);
        return 0;
    }
    if (lua_pcall(L, 0, 1, 0) != LUA_OK) {
        lua_settop(L, base);
        return 0;
    }
    if (lua_type(L, -1) != LUA_TSTRING) {
        lua_settop(L, base);
        return 0;
    }
    const char *s = lua_tostring(L, -1);
    snprintf(out, out_sz, "%s", s);
    lua_settop(L, base);
    return 1;
}

// Run a Lua chunk that must `return` an integer.
static int eval_int(lua_State *L, const char *chunk, long long *out) {
    int base = lua_gettop(L);
    if (luaL_loadstring(L, chunk) != LUA_OK) { lua_settop(L, base); return 0; }
    if (lua_pcall(L, 0, 1, 0) != LUA_OK)     { lua_settop(L, base); return 0; }
    if (!lua_isinteger(L, -1))               { lua_settop(L, base); return 0; }
    *out = (long long)lua_tointeger(L, -1);
    lua_settop(L, base);
    return 1;
}

// ---------------------------------------------------------------------------
// Sanity: label -> value must work (baseline, expected green everywhere)
// ---------------------------------------------------------------------------

TEST(enum_label_lookup_works) {
    lua_State *L = make_state();
    ASSERT_NOT_NULL(L);

    long long v = -1;
    ASSERT_TRUE(eval_int(L, "return Ext.Enums.DamageType.Fire.Value", &v));
    ASSERT_EQ(v, 7);

    ASSERT_TRUE(eval_int(L, "return Ext.Enums.AbilityId.Constitution.Value", &v));
    ASSERT_EQ(v, 3);

    lua_close(L);
}

// ---------------------------------------------------------------------------
// The defect: integer key -> label (reverse lookup)
//
// enum_type_index() tests lua_isstring() first. lua_isstring() returns true for
// NUMBERS as well as strings (Lua's number->string coercion), so an integer key
// is swallowed by the label branch, enum_find_value(type, "7") fails, and the
// `else if (lua_isinteger(...))` reverse-lookup branch is never reached.
// Result: Ext.Enums.<Type>[n] is always nil.
// ---------------------------------------------------------------------------

TEST(enum_integer_key_returns_userdata) {
    lua_State *L = make_state();
    ASSERT_NOT_NULL(L);

    char t[64] = {0};
    ASSERT_TRUE(eval_string(L, "return type(Ext.Enums.DamageType[7])", t, sizeof t));
    ASSERT_STR_EQ(t, "userdata");

    lua_close(L);
}

TEST(enum_integer_key_resolves_label) {
    lua_State *L = make_state();
    ASSERT_NOT_NULL(L);

    char label[64] = {0};
    ASSERT_TRUE(eval_string(L, "return Ext.Enums.DamageType[7].Label", label, sizeof label));
    ASSERT_STR_EQ(label, "Fire");

    ASSERT_TRUE(eval_string(L, "return Ext.Enums.DamageType[1].Label", label, sizeof label));
    ASSERT_STR_EQ(label, "Slashing");

    ASSERT_TRUE(eval_string(L, "return Ext.Enums.AbilityId[6].Label", label, sizeof label));
    ASSERT_STR_EQ(label, "Charisma");

    ASSERT_TRUE(eval_string(L, "return Ext.Enums.SkillId[16].Label", label, sizeof label));
    ASSERT_STR_EQ(label, "Perception");

    lua_close(L);
}

TEST(enum_integer_key_roundtrips) {
    lua_State *L = make_state();
    ASSERT_NOT_NULL(L);

    long long v = -1;
    ASSERT_TRUE(eval_int(L,
        "return Ext.Enums.DamageType[Ext.Enums.DamageType.Radiant.Value].Value", &v));
    ASSERT_EQ(v, 12);

    lua_close(L);
}

// Value 0 is a legal DamageType ("None") and must not be confused with "absent".
TEST(enum_integer_key_zero_resolves) {
    lua_State *L = make_state();
    ASSERT_NOT_NULL(L);

    char label[64] = {0};
    ASSERT_TRUE(eval_string(L, "return Ext.Enums.DamageType[0].Label", label, sizeof label));
    ASSERT_STR_EQ(label, "None");

    lua_close(L);
}

// Same defect on the bitfield path: Ext.Enums.<Bitfield>[mask].
TEST(bitfield_integer_key_returns_userdata) {
    lua_State *L = make_state();
    ASSERT_NOT_NULL(L);

    char t[64] = {0};
    ASSERT_TRUE(eval_string(L, "return type(Ext.Enums.AttributeFlags[0x2])", t, sizeof t));
    ASSERT_STR_EQ(t, "userdata");

    long long v = -1;
    ASSERT_TRUE(eval_int(L, "return Ext.Enums.AttributeFlags[0x6].__Value", &v));
    ASSERT_EQ(v, 6);

    lua_close(L);
}

// ---------------------------------------------------------------------------
// Negative cases: must stay nil after the fix.
// ---------------------------------------------------------------------------

TEST(enum_unknown_label_is_nil) {
    lua_State *L = make_state();
    ASSERT_NOT_NULL(L);

    char t[64] = {0};
    ASSERT_TRUE(eval_string(L, "return type(Ext.Enums.DamageType.NotAThing)", t, sizeof t));
    ASSERT_STR_EQ(t, "nil");

    lua_close(L);
}

TEST(enum_out_of_range_integer_is_nil) {
    lua_State *L = make_state();
    ASSERT_NOT_NULL(L);

    char t[64] = {0};
    ASSERT_TRUE(eval_string(L, "return type(Ext.Enums.DamageType[999])", t, sizeof t));
    ASSERT_STR_EQ(t, "nil");

    lua_close(L);
}

// A *string* "7" is not a label and must not be coerced into the numeric path.
TEST(enum_numeric_string_key_is_nil) {
    lua_State *L = make_state();
    ASSERT_NOT_NULL(L);

    char t[64] = {0};
    ASSERT_TRUE(eval_string(L, "return type(Ext.Enums.DamageType['7'])", t, sizeof t));
    ASSERT_STR_EQ(t, "nil");

    lua_close(L);
}

// ---------------------------------------------------------------------------
// Same ordering defect in the enum __eq and the bitfield operand reader
// (src/enum/enum_lua.c, src/enum/bitfield_lua.c): lua_isstring() swallowed a
// numeric operand, so a bitwise op against a plain integer mask looked up the
// label "4" and failed, and __eq against an integer compared the label "7".
// Lua only dispatches __eq when both operands are userdata, so the enum test
// reaches the metamethod through the metatable, as a mod would via
// getmetatable(v).__eq.
// ---------------------------------------------------------------------------

TEST(bitfield_bor_accepts_integer_operand) {
    lua_State *L = make_state();
    ASSERT_NOT_NULL(L);

    long long v = -1;
    ASSERT_TRUE(eval_int(L, "return (Ext.Enums.AttributeFlags[0x2] | 0x4).__Value", &v));
    ASSERT_EQ(v, 6);

    ASSERT_TRUE(eval_int(L, "return (Ext.Enums.AttributeFlags[0x6] & 0x4).__Value", &v));
    ASSERT_EQ(v, 4);

    ASSERT_TRUE(eval_int(L, "return (Ext.Enums.AttributeFlags[0x6] ~ 0x2).__Value", &v));
    ASSERT_EQ(v, 4);

    lua_close(L);
}

// A numeric *string* is not a label and must not be coerced into a mask.
TEST(bitfield_bor_numeric_string_is_rejected) {
    lua_State *L = make_state();
    ASSERT_NOT_NULL(L);

    char t[64] = {0};
    ASSERT_TRUE(eval_string(L,
        "local ok = pcall(function() return Ext.Enums.AttributeFlags[0x2] | '4' end)\n"
        "return tostring(ok)", t, sizeof t));
    ASSERT_STR_EQ(t, "false");

    lua_close(L);
}

TEST(enum_eq_accepts_integer_operand) {
    lua_State *L = make_state();
    ASSERT_NOT_NULL(L);

    char t[64] = {0};
    ASSERT_TRUE(eval_string(L,
        "local fire = Ext.Enums.DamageType.Fire\n"
        "local eq = getmetatable(fire).__eq\n"
        "return tostring(eq(fire, 7)) .. ',' .. tostring(eq(fire, 8)) .. ','\n"
        "    .. tostring(eq(fire, 'Fire')) .. ',' .. tostring(eq(fire, '7'))",
        t, sizeof t));
    ASSERT_STR_EQ(t, "true,false,true,false");

    lua_close(L);
}

// ---------------------------------------------------------------------------

void register_enum_ext_tests(void) {
    printf("--- enum_ext (Ext.Enums lookup) ---\n");
    RUN_TEST(enum_label_lookup_works);
    RUN_TEST(enum_integer_key_returns_userdata);
    RUN_TEST(enum_integer_key_resolves_label);
    RUN_TEST(enum_integer_key_roundtrips);
    RUN_TEST(enum_integer_key_zero_resolves);
    RUN_TEST(bitfield_integer_key_returns_userdata);
    RUN_TEST(enum_unknown_label_is_nil);
    RUN_TEST(enum_out_of_range_integer_is_nil);
    RUN_TEST(enum_numeric_string_key_is_nil);
    RUN_TEST(bitfield_bor_accepts_integer_operand);
    RUN_TEST(bitfield_bor_numeric_string_is_rejected);
    RUN_TEST(enum_eq_accepts_integer_operand);
}
