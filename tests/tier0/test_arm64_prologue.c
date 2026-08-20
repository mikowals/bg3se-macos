/*
 * Tier 0 tests for src/hooks/arm64_decode.c — arm64_analyze_prologue().
 *
 * Regression guard for the ExecuteStatsFunctors AttackTarget SIGSEGV: the
 * analyzer used to return a NONZERO safe_hook_offset whenever a PC-relative
 * instruction appeared LATER in the prologue (e.g. a `bl` at +0x34), even when
 * the 16-byte entry window was clean. That made the installer hook at target+4,
 * skipping instruction 0 (`stp x28,x27,[sp,#-0x60]!`); the trampoline re-ran that
 * stack push, double-decrementing SP and corrupting the callee frame. The fix
 * prefers offset 0 whenever the entry window has no PC-relative instruction.
 *
 * No game process needed — synthetic instruction buffers.
 */

#include "test_harness.h"
#include "arm64_decode.h"
#include "arm64_hook.h"

#define NOP  0xD503201Fu   /* not PC-relative */
#define BL0  0x94000000u   /* bl #0 — PC-relative (like `bl HitDesc::HitDesc`) */
#define ADRP 0x90000000u   /* adrp x0, #0 — PC-relative */
#define LDR0 0xF9400000u   /* ldr x0, [x0] — pairs with the adrp */
#define STP_PRE_SP 0xA9BA6FFCu /* stp x28,x27,[sp,#-0x60]! — real frame push (not PC-relative) */

/* The exact shape that crashed: frame-push at 0, non-PC-relative setup, a `bl`
 * deep in the prologue. safe_hook_offset MUST be 0 (not 4). */
TEST(prefers_offset_zero_despite_later_bl) {
    uint32_t prologue[16] = {
        STP_PRE_SP, NOP, NOP, NOP,   /* entry window — all non-PC-relative */
        NOP, NOP, NOP, NOP,
        NOP, NOP, NOP, NOP,
        NOP, BL0, NOP, NOP,          /* bl at index 13 (offset 0x34) */
    };
    ARM64PrologueAnalysis a;
    ASSERT_TRUE(arm64_analyze_prologue(prologue, 16, &a));
    ASSERT_EQ(a.safe_hook_offset, 0);
}

/* A totally clean prologue is offset 0 too. */
TEST(clean_prologue_offset_zero) {
    uint32_t prologue[16];
    for (int i = 0; i < 16; i++) prologue[i] = NOP;
    ARM64PrologueAnalysis a;
    ASSERT_TRUE(arm64_analyze_prologue(prologue, 16, &a));
    ASSERT_EQ(a.safe_hook_offset, 0);
}

/* A LEADING PC-relative instruction (adrp) DOES need a nonzero offset — the entry
 * window is dirty and the trampoline cannot relocate the adrp, so the fix must NOT
 * force offset 0 here. Confirms the offset-0 preference doesn't over-reach. */
TEST(leading_adrp_skips_forward) {
    uint32_t prologue[16] = {
        ADRP, LDR0, NOP, NOP,
        NOP, NOP, NOP, NOP,
        NOP, NOP, NOP, NOP,
        NOP, NOP, NOP, NOP,
    };
    ARM64PrologueAnalysis a;
    ASSERT_TRUE(arm64_analyze_prologue(prologue, 16, &a));
    ASSERT_NE(a.safe_hook_offset, 0);
    ASSERT_TRUE(a.safe_hook_offset >= 8);   /* past the adrp+ldr pair */
}

/* ── branch island (far-jump stub) ─────────────────────────────────────────────
 * The island exists so a hook patches ONE atomic 4-byte branch into live game
 * code instead of memcpy'ing four instructions, which cannot be written
 * atomically and leaves a torn-instruction window for other threads. */

TEST(alloc_near_lands_within_branch_reach) {
    /* Target something real and mapped: this function's own code address. */
    void* target = (void*)(uintptr_t)&test_alloc_near_lands_within_branch_reach;
    void* got = arm64_alloc_near(target, 16);
    ASSERT_TRUE(got != NULL);

    int64_t dist = (int64_t)got - (int64_t)target;
    if (dist < 0) dist = -dist;
    ASSERT_TRUE(dist < 128LL * 1024 * 1024);
    arm64_free_near(got, 16);
}

void register_arm64_prologue_tests(void) {
    printf("[arm64_prologue]\n");
    RUN_TEST(prefers_offset_zero_despite_later_bl);
    RUN_TEST(clean_prologue_offset_zero);
    RUN_TEST(leading_adrp_skips_forward);
    RUN_TEST(alloc_near_lands_within_branch_reach);
}
