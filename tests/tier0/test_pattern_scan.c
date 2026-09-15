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


/* ── AUDIT ADDITIONS ─────────────────────────────────────────────── */

/* GAP: nothing exercised parse_pattern's rejection paths. Deleting both
 * `return NULL` validation arms (invalid hex + invalid character) left the
 * whole suite green, so a garbage pattern string would have silently produced
 * a pattern that scans for whatever sscanf happened to leave behind. */
TEST(parse_rejects_non_hex) {
    ASSERT_NULL(parse_pattern("ZZ"));
    ASSERT_NULL(parse_pattern("48 ZZ"));
    ASSERT_NULL(parse_pattern("48 8G"));
    ASSERT_NULL(parse_pattern("**"));
    ASSERT_NULL(parse_pattern("48-8D"));
}

TEST(parse_rejects_odd_nibble) {
    ASSERT_NULL(parse_pattern("4"));
    ASSERT_NULL(parse_pattern("48 8"));
    ASSERT_NULL(parse_pattern("48 8D 0"));
}

TEST(parse_rejects_lone_question_mark) {
    ASSERT_NULL(parse_pattern("?"));
    ASSERT_NULL(parse_pattern("48 ? 05"));
}

TEST(parse_whitespace_only_is_null) {
    ASSERT_NULL(parse_pattern("   "));
    ASSERT_NULL(parse_pattern("\t"));
}

/* Wildcard bytes must be zeroed as well as masked — a caller that hexdumps
 * pat->bytes would otherwise print uninitialised malloc contents. */
TEST(parse_wildcard_zeroes_byte) {
    BytePattern *p = parse_pattern("?? ?? ??");
    ASSERT_NOT_NULL(p);
    ASSERT_EQ(p->length, 3u);
    for (size_t i = 0; i < 3; i++) {
        ASSERT_EQ(p->mask[i], 0x00);
        ASSERT_EQ(p->bytes[i], 0x00);
    }
    free_pattern(p);
}

TEST(parse_lowercase_hex_matches_uppercase) {
    BytePattern *lo = parse_pattern("de ad be ef");
    BytePattern *hi = parse_pattern("DE AD BE EF");
    ASSERT_NOT_NULL(lo);
    ASSERT_NOT_NULL(hi);
    ASSERT_EQ(lo->length, hi->length);
    ASSERT_EQ(memcmp(lo->bytes, hi->bytes, lo->length), 0);
    free_pattern(lo);
    free_pattern(hi);
}

TEST(find_null_inputs_are_null) {
    unsigned char buf[] = { 0xAA, 0xBB };
    BytePattern *p = parse_pattern("AA BB");
    ASSERT_NOT_NULL(p);
    ASSERT_NULL(find_pattern(NULL, sizeof(buf), p));
    ASSERT_NULL(find_pattern(buf, sizeof(buf), NULL));
    ASSERT_NULL(find_pattern(buf, 0, p));
    free_pattern(p);
}

/* find_pattern must return the FIRST match, and must not run off the end of
 * the buffer looking for a later one. */
TEST(find_returns_first_match_only) {
    unsigned char buf[] = { 0xCA, 0xFE, 0x00, 0xCA, 0xFE };
    BytePattern *p = parse_pattern("CA FE");
    ASSERT_NOT_NULL(p);
    ASSERT_EQ(find_pattern(buf, sizeof(buf), p), (void *)&buf[0]);
    ASSERT_EQ(find_pattern(buf + 1, sizeof(buf) - 1, p), (void *)&buf[3]);
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
    RUN_TEST(parse_rejects_non_hex);
    RUN_TEST(parse_rejects_odd_nibble);
    RUN_TEST(parse_rejects_lone_question_mark);
    RUN_TEST(parse_whitespace_only_is_null);
    RUN_TEST(parse_wildcard_zeroes_byte);
    RUN_TEST(parse_lowercase_hex_matches_uppercase);
    RUN_TEST(find_null_inputs_are_null);
    RUN_TEST(find_returns_first_match_only);
}
