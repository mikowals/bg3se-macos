/*
 * Tier 0 tests for Osiris handle encoding/decoding.
 *
 * Tests osi_encode_handle / osi_decode_func_id / osi_decode_func_type
 * from src/osiris/osiris_types.h. Pure inline bitwise — zero deps.
 */

#include "test_harness.h"
#include "osiris_types.h"

/* ── Type roundtrip ──────────────────────────────────────────────── */

TEST(encode_decode_type_roundtrip) {
    for (uint32_t t = 0; t < 8; t++) {
        uint32_t h = osi_encode_handle(t, 0, 100, 0);
        ASSERT_EQ(osi_decode_func_type(h), (uint8_t)t);
    }
}

/* ── Low type (< 4): 25-bit funcIndex preserved ──────���──────────── */

TEST(encode_decode_funcid_low_type) {
    uint32_t funcIndex = 12345;
    uint32_t h = osi_encode_handle(1, 0, funcIndex, 0);
    ASSERT_EQ(osi_decode_func_type(h), 1);
    ASSERT_EQ(osi_decode_func_id(h), funcIndex);
}

TEST(encode_decode_funcid_low_type_max) {
    uint32_t funcIndex = 0x1FFFFFF;  /* max 25-bit */
    uint32_t h = osi_encode_handle(2, 0, funcIndex, 0);
    ASSERT_EQ(osi_decode_func_id(h), funcIndex);
}

/* ── High type (>= 4): 17-bit funcIndex ──��───────────────────────── */

TEST(encode_decode_funcid_high_type) {
    uint32_t funcIndex = 42;
    uint32_t h = osi_encode_handle(5, 0xFF, funcIndex, 0);
    ASSERT_EQ(osi_decode_func_type(h), 5);
    ASSERT_EQ(osi_decode_func_id(h), funcIndex);
}

TEST(encode_decode_funcid_high_type_max) {
    uint32_t funcIndex = 0x1FFFF;  /* max 17-bit */
    uint32_t h = osi_encode_handle(7, 0, funcIndex, 0);
    ASSERT_EQ(osi_decode_func_id(h), funcIndex);
}

/* ── Part4 bit ──────────��────────────────────────────────────────── */

TEST(encode_part4_sets_bit31) {
    uint32_t h = osi_encode_handle(1, 0, 50, 1);
    ASSERT_TRUE(h & (1u << 31));
}

TEST(encode_no_part4_clears_bit31) {
    uint32_t h = osi_encode_handle(1, 0, 50, 0);
    ASSERT_FALSE(h & (1u << 31));
}

/* ── High-bit funcIndex ──────────────────────────────────────────── */

TEST(encode_decode_high_bit_funcid) {
    uint32_t funcIndex = (1u << 24) | 7;
    uint32_t h = osi_encode_handle(2, 0, funcIndex, 0);
    ASSERT_EQ(osi_decode_func_id(h), funcIndex);
}

/* ── Registration ──────────��──────────────────���──────────────────── */

void register_osiris_handle_tests(void) {
    printf("[osiris_handles]\n");
    RUN_TEST(encode_decode_type_roundtrip);
    RUN_TEST(encode_decode_funcid_low_type);
    RUN_TEST(encode_decode_funcid_low_type_max);
    RUN_TEST(encode_decode_funcid_high_type);
    RUN_TEST(encode_decode_funcid_high_type_max);
    RUN_TEST(encode_part4_sets_bit31);
    RUN_TEST(encode_no_part4_clears_bit31);
    RUN_TEST(encode_decode_high_bit_funcid);
}
