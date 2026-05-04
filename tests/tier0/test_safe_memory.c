/*
 * Tier 0 tests for src/core/safe_memory.c
 *
 * Tests safe_memory_read edge cases and GPU region detection.
 * All tests run without BG3.
 */

#include "test_harness.h"
#include "safe_memory.h"

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
}
