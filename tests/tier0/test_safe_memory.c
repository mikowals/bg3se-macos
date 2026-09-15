/*
 * Tier 0 tests for src/core/safe_memory.c
 *
 * Tests safe_memory_read edge cases and GPU region detection.
 * All tests run without BG3.
 */

#include "test_harness.h"
#include "safe_memory.h"

#include <mach/mach_vm.h>

/* ── GPU region detection (pure address range, no mach_vm) ─────── */

TEST(gpu_region_inside) {
    ASSERT_TRUE(safe_memory_is_gpu_region(0x4900000000ULL));
}

TEST(gpu_region_start_boundary) {
    ASSERT_TRUE(safe_memory_is_gpu_region(0x1000000000ULL));
}

TEST(gpu_region_below) {
    ASSERT_FALSE(safe_memory_is_gpu_region(0x0FFFFFFFULL));
}

TEST(gpu_region_above) {
    ASSERT_FALSE(safe_memory_is_gpu_region(0x7000000000ULL));
}

TEST(gpu_region_zero) {
    ASSERT_FALSE(safe_memory_is_gpu_region(0));
}

/* ── safe_memory_read null/edge cases ────────────────────────────── */

TEST(read_null_address) {
    uint64_t val = 0;
    ASSERT_FALSE(safe_memory_read(0, &val, sizeof(val)));
}

TEST(read_small_address) {
    uint64_t val = 0;
    ASSERT_FALSE(safe_memory_read(0x100, &val, sizeof(val)));
}

TEST(read_null_dest) {
    ASSERT_FALSE(safe_memory_read(0x100000000ULL, NULL, 8));
}

TEST(read_zero_size) {
    uint64_t val = 0;
    ASSERT_FALSE(safe_memory_read(0x100000000ULL, &val, 0));
}

TEST(read_gpu_region_rejected) {
    uint64_t val = 0;
    ASSERT_FALSE(safe_memory_read(0x4900000000ULL, &val, sizeof(val)));
}

/* ── safe_memory_read_string ─────────────────────────────────────── */

TEST(read_string_null_addr) {
    char buf[64];
    ASSERT_FALSE(safe_memory_read_string(0, buf, sizeof(buf)));
}

TEST(read_string_null_buf) {
    ASSERT_FALSE(safe_memory_read_string(0x100000000ULL, NULL, 64));
}

/* ── safe_memory_read own stack (positive case) ──────────────────── */

TEST(read_valid_stack) {
    uint64_t val = 0xDEADBEEFCAFEBABEULL;
    uint64_t out = 0;
    ASSERT_TRUE(safe_memory_read((mach_vm_address_t)&val, &out, sizeof(out)));
    ASSERT_EQ(out, 0xDEADBEEFCAFEBABEULL);
}

TEST(read_u32_valid_stack) {
    uint32_t val = 0xF00DCAFE;
    uint32_t out = 0;
    ASSERT_TRUE(safe_memory_read_u32((mach_vm_address_t)&val, &out));
    ASSERT_EQ(out, 0xF00DCAFE);
}

TEST(read_pointer_valid_stack) {
    int x = 42;
    int *ptr = &x;
    void *out = NULL;
    ASSERT_TRUE(safe_memory_read_pointer((mach_vm_address_t)&ptr, &out));
    ASSERT_EQ(out, (void *)ptr);
}

/* ── safe_memory_check_address ───────────────────────────────────── */

TEST(check_address_null) {
    SafeMemoryInfo info = safe_memory_check_address(0);
    ASSERT_FALSE(info.is_valid);
}

TEST(check_address_own_stack) {
    int x = 1;
    SafeMemoryInfo info = safe_memory_check_address((mach_vm_address_t)&x);
    ASSERT_TRUE(info.is_valid);
    ASSERT_TRUE(info.is_readable);
}


/* ── AUDIT ADDITIONS ─────────────────────────────────────────────── */

/* read_gpu_region_rejected below cannot distinguish the carveout pre-check
 * from the kernel: 0x4900000000 is unmapped in this process, so
 * safe_memory_read() rejects it either way, and the whole 0x10..0x70 GB range
 * is reserved (mach_vm_allocate returns KERN_NO_SPACE at every probe). The
 * pre-check stays mutation-equivalent offline. What IS pinnable is the
 * predicate itself, including the exact faulting addresses recorded in
 * safe_memory.c's comment. */
TEST(gpu_region_pins_documented_range) {
    /* Addresses taken from real crashes, per the comment in safe_memory.c. */
    ASSERT_TRUE(safe_memory_is_gpu_region(0x49000004a6ULL));
    ASSERT_TRUE(safe_memory_is_gpu_region(0x4900000000ULL));
    /* Inclusive start, exclusive end — both boundaries and their neighbours. */
    ASSERT_FALSE(safe_memory_is_gpu_region(0x1000000000ULL - 1));
    ASSERT_TRUE(safe_memory_is_gpu_region(0x1000000000ULL));
    ASSERT_TRUE(safe_memory_is_gpu_region(0x7000000000ULL - 1));
    ASSERT_FALSE(safe_memory_is_gpu_region(0x7000000000ULL));
}

/* check_address_own_stack only asserts is_valid/is_readable, so deleting the
 * "address actually inside the returned region" check in
 * safe_memory_check_address() leaves the whole suite green — while the
 * function then reports gaps between regions as valid, which is the one thing
 * it exists to prevent. Pin the region fields it returns. */
TEST(check_address_region_actually_contains_address) {
    int x = 1;
    mach_vm_address_t addr = (mach_vm_address_t)&x;
    SafeMemoryInfo info = safe_memory_check_address(addr);
    ASSERT_TRUE(info.is_valid);
    ASSERT_TRUE(info.region_size > 0);
    ASSERT_TRUE(addr >= info.region_start);
    ASSERT_TRUE(addr < info.region_start + info.region_size);
}

/* mach_vm_region returns the region at-or-AFTER the queried address, so an
 * address sitting in a hole is reported by the kernel together with the next
 * mapped region above it. safe_memory_check_address must reject it. Build the
 * hole deterministically: allocate two pages, punch out the first, and query
 * the punched-out page — a region still exists immediately above it. */
TEST(check_address_in_unmapped_gap_is_invalid) {
    mach_vm_size_t page = (mach_vm_size_t)vm_page_size;
    mach_vm_address_t base = 0;
    ASSERT_EQ(mach_vm_allocate(mach_task_self(), &base, page * 2,
                               VM_FLAGS_ANYWHERE), KERN_SUCCESS);
    /* Both pages mapped: the low page is valid. */
    ASSERT_TRUE(safe_memory_check_address(base).is_valid);

    ASSERT_EQ(mach_vm_deallocate(mach_task_self(), base, page), KERN_SUCCESS);
    /* Low page is now a hole with a live region directly above it. */
    SafeMemoryInfo hole = safe_memory_check_address(base);
    ASSERT_FALSE(hole.is_valid);
    /* The surviving page is still valid, so this is a containment check and
     * not just "the allocation went away". */
    ASSERT_TRUE(safe_memory_check_address(base + page).is_valid);

    mach_vm_deallocate(mach_task_self(), base + page, page);
}

/* GAP: both existing read_string tests are negative (NULL addr / NULL buf), so
 * the copy loop itself was untested. These pin the observable contract: the
 * copy, truncation to max_len-1 characters, and the trailing NUL, with bytes
 * past max_len left untouched. Note what they do NOT catch: relaxing the loop
 * bound to `i < max_len` is output-equivalent, because the loop only ever
 * writes buffer[i] for i < max_len and the final `buffer[max_len - 1] = '\0'`
 * rewrites the last byte either way.
 *
 * Canaries live in the same backing array as the buffer (logical size smaller
 * than the array) so an overflowing mutant is observed as defined behavior. */
TEST(read_string_copies_and_terminates) {
    static const char src[] = "BG3MCM";
    char backing[16 + 8];
    memset(backing, 0x7E, sizeof(backing));

    ASSERT_TRUE(safe_memory_read_string((mach_vm_address_t)src, backing, 16));
    ASSERT_STR_EQ(backing, "BG3MCM");
    for (size_t i = 16; i < sizeof(backing); i++) {
        ASSERT_EQ(backing[i], (char)0x7E);
    }
}

TEST(read_string_truncates_within_max_len) {
    static const char src[] = "ABCDEFGH";
    char backing[4 + 8];
    memset(backing, 0x7E, sizeof(backing));

    ASSERT_TRUE(safe_memory_read_string((mach_vm_address_t)src, backing, 4));
    ASSERT_STR_EQ(backing, "ABC");        /* max_len-1 chars kept */
    ASSERT_EQ(backing[3], '\0');          /* always NUL-terminated */
    for (size_t i = 4; i < sizeof(backing); i++) {
        ASSERT_EQ(backing[i], (char)0x7E);   /* never writes buf[max_len] */
    }
}

TEST(read_string_exact_fit_no_overflow) {
    static const char src[] = "abc";
    char backing[4 + 8];
    memset(backing, 0x7E, sizeof(backing));

    ASSERT_TRUE(safe_memory_read_string((mach_vm_address_t)src, backing, 4));
    ASSERT_STR_EQ(backing, "abc");
    for (size_t i = 4; i < sizeof(backing); i++) {
        ASSERT_EQ(backing[i], (char)0x7E);
    }
}

/* ── Registration ────────────────────────────────────────────────── */

void register_safe_memory_tests(void) {
    printf("[safe_memory]\n");
    RUN_TEST(gpu_region_inside);
    RUN_TEST(gpu_region_start_boundary);
    RUN_TEST(gpu_region_below);
    RUN_TEST(gpu_region_above);
    RUN_TEST(gpu_region_zero);
    RUN_TEST(read_null_address);
    RUN_TEST(read_small_address);
    RUN_TEST(read_null_dest);
    RUN_TEST(read_zero_size);
    RUN_TEST(read_gpu_region_rejected);
    RUN_TEST(read_string_null_addr);
    RUN_TEST(read_string_null_buf);
    RUN_TEST(read_valid_stack);
    RUN_TEST(read_u32_valid_stack);
    RUN_TEST(read_pointer_valid_stack);
    RUN_TEST(check_address_null);
    RUN_TEST(check_address_own_stack);
    RUN_TEST(gpu_region_pins_documented_range);
    RUN_TEST(check_address_region_actually_contains_address);
    RUN_TEST(check_address_in_unmapped_gap_is_invalid);
    RUN_TEST(read_string_copies_and_terminates);
    RUN_TEST(read_string_truncates_within_max_len);
    RUN_TEST(read_string_exact_fit_no_overflow);
}
