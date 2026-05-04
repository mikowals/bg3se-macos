/*
 * BG3SE-macOS Tier 0 Test Harness — shared macros.
 * Included by each test_*.c file.
 */

#ifndef BG3SE_TEST_HARNESS_H
#define BG3SE_TEST_HARNESS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

extern int g_passed;
extern int g_failed;
extern jmp_buf g_test_jmp;

#define TEST(name) static void test_##name(void)

#define RUN_TEST(name) do { \
    if (setjmp(g_test_jmp) == 0) { \
        test_##name(); \
        g_passed++; \
        printf("  PASS: %s\n", #name); \
    } else { \
        g_failed++; \
        printf("  FAIL: %s\n", #name); \
    } \
} while(0)

#define ASSERT_TRUE(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "    assertion failed: %s  (%s:%d)\n", \
                #cond, __FILE__, __LINE__); \
        longjmp(g_test_jmp, 1); \
    } \
} while(0)

#define ASSERT_FALSE(cond)    ASSERT_TRUE(!(cond))
#define ASSERT_EQ(a, b)       ASSERT_TRUE((a) == (b))
#define ASSERT_NE(a, b)       ASSERT_TRUE((a) != (b))
#define ASSERT_NULL(p)        ASSERT_TRUE((p) == NULL)
#define ASSERT_NOT_NULL(p)    ASSERT_TRUE((p) != NULL)
#define ASSERT_STR_EQ(a, b)   ASSERT_TRUE(strcmp((a), (b)) == 0)

#endif /* BG3SE_TEST_HARNESS_H */
