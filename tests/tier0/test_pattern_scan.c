/*
 * Tier 0 tests for src/osiris/pattern_scan.c
 *
 * Tests parse_pattern and find_pattern with synthetic buffers.
 * No game process needed.
 */

#include "test_harness.h"
#include "pattern_scan.h"

/* ── parse_pattern ───────────────────────────────────────────────── */

TEST(parse_null) {
    ASSERT_NULL(parse_pattern(NULL));
}

TEST(parse_empty) {
    ASSERT_NULL(parse_pattern(""));
}

TEST(parse_simple) {
    BytePattern *p = parse_pattern("48 8D 05");
    ASSERT_NOT_NULL(p);
    ASSERT_EQ(p->length, 3u);
    ASSERT_EQ(p->bytes[0], 0x48);
    ASSERT_EQ(p->bytes[1], 0x8D);
    ASSERT_EQ(p->bytes[2], 0x05);
    ASSERT_EQ(p->mask[0], 0xFF);
    ASSERT_EQ(p->mask[1], 0xFF);
    ASSERT_EQ(p->mask[2], 0xFF);
    free_pattern(p);
}

TEST(parse_wildcard) {
    BytePattern *p = parse_pattern("48 ?? 05");
    ASSERT_NOT_NULL(p);
    ASSERT_EQ(p->length, 3u);
    ASSERT_EQ(p->mask[0], 0xFF);
    ASSERT_EQ(p->mask[1], 0x00);
    ASSERT_EQ(p->mask[2], 0xFF);
    free_pattern(p);
}

TEST(parse_single_byte) {
    BytePattern *p = parse_pattern("FF");
    ASSERT_NOT_NULL(p);
    ASSERT_EQ(p->length, 1u);
    ASSERT_EQ(p->bytes[0], 0xFF);
    free_pattern(p);
}

/* ── find_pattern ────────────────────────────────────────────────── */

TEST(find_exact_match) {
    unsigned char buf[] = { 0x00, 0xDE, 0xAD, 0x00 };
    BytePattern *p = parse_pattern("DE AD");
    ASSERT_NOT_NULL(p);
    void *hit = find_pattern(buf, sizeof(buf), p);
    ASSERT_NOT_NULL(hit);
    ASSERT_EQ(hit, (void *)&buf[1]);
    free_pattern(p);
}

TEST(find_wildcard_match) {
    unsigned char buf[] = { 0x00, 0xDE, 0x99, 0xBE, 0x00 };
    BytePattern *p = parse_pattern("DE ?? BE");
    ASSERT_NOT_NULL(p);
    void *hit = find_pattern(buf, sizeof(buf), p);
    ASSERT_NOT_NULL(hit);
    ASSERT_EQ(hit, (void *)&buf[1]);
    free_pattern(p);
}

TEST(find_no_match) {
    unsigned char buf[] = { 0x01, 0x02, 0x03, 0x04 };
    BytePattern *p = parse_pattern("FF FF");
    ASSERT_NOT_NULL(p);
    void *hit = find_pattern(buf, sizeof(buf), p);
    ASSERT_NULL(hit);
    free_pattern(p);
}

TEST(find_at_end) {
    unsigned char buf[] = { 0x00, 0x00, 0xCA, 0xFE };
    BytePattern *p = parse_pattern("CA FE");
    ASSERT_NOT_NULL(p);
    void *hit = find_pattern(buf, sizeof(buf), p);
    ASSERT_NOT_NULL(hit);
    ASSERT_EQ(hit, (void *)&buf[2]);
    free_pattern(p);
}

TEST(find_buffer_too_small) {
    unsigned char buf[] = { 0xAA };
    BytePattern *p = parse_pattern("AA BB");
    ASSERT_NOT_NULL(p);
    void *hit = find_pattern(buf, sizeof(buf), p);
    ASSERT_NULL(hit);
    free_pattern(p);
}

/* ── Registration ────────────────────────────────────────────────── */

void register_pattern_scan_tests(void) {
    printf("[pattern_scan]\n");
    RUN_TEST(parse_null);
    RUN_TEST(parse_empty);
    RUN_TEST(parse_simple);
    RUN_TEST(parse_wildcard);
    RUN_TEST(parse_single_byte);
    RUN_TEST(find_exact_match);
    RUN_TEST(find_wildcard_match);
    RUN_TEST(find_no_match);
    RUN_TEST(find_at_end);
    RUN_TEST(find_buffer_too_small);
}
