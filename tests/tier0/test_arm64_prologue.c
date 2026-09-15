/*
 * Tier 0 tests for src/hooks/arm64_decode.c — arm64_analyze_prologue().
 *
 * Regression guard for the safe_hook_offset +4 defect: the analyzer used to
 * return a NONZERO offset whenever a PC-relative instruction appeared LATER in
 * the prologue (e.g. a `bl` at +0x34), even when the 16-byte entry window was
 * clean. That made arm64_safe_hook() install at target+4, skipping instruction
 * 0 (a frame push such as `stp x28,x27,[sp,#-0x60]!`); the trampoline then
 * re-ran that push, double-decrementing SP and corrupting the callee frame.
 * The fix prefers offset 0 whenever the entry window has no PC-relative
 * instruction, and FAILS CLOSED (-1) when it has one: the forward skip copied
 * the skipped instructions into the trampoline unrelocated, so an ADRP/ADR/
 * LDR-literal evaluated against the wrong PC and a branch went to the wrong
 * place (issue #106). arm64_safe_hook() now refuses such targets and the
 * caller takes its Dobby fallback.
 *
 * Attribution: the live consumer whose offset moves (+4 -> 0) is the
 * MessageFactory::GetFreeMessage net hook (src/network/net_hooks.c), which is
 * installed through arm64_safe_hook(). The ExecuteStatsFunctors hooks
 * (AttackTarget et al., src/stats/functor_hooks.c) use plain DobbyHook() and
 * never consult this analyzer; their earlier SIGSEGV is a separate matter
 * (docs/bugs/wave2-functor-crash-analysis.md).
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
#define B0     0x14000000u   /* b #0 */
#define CBZ0   0x34000000u   /* cbz w0, #0 */
#define CBNZ0  0x35000000u   /* cbnz w0, #0 */
#define TBZ0   0x36000000u   /* tbz w0, #0, #0 */
#define TBNZ0  0x37000000u   /* tbnz w0, #0, #0 — bit 24 set; was missed by the 0x7F mask */
#define BCOND0 0x54000000u   /* b.eq #0 */
#define ADR0   0x10000000u   /* adr x0, #0 */
#define LDRLIT 0x58000000u   /* ldr x0, literal — PC-relative load */

/* The GetFreeMessage shape: frame-push at 0, non-PC-relative setup, a `bl`
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

/* A PC-relative instruction ANYWHERE in the four-instruction entry window
 * makes the target unhookable by this mechanism: no relocation exists, so
 * the analyzer must fail closed (-1) rather than skip forward over it. Every
 * form the two classifiers know, at every index 0..3. */
TEST(dirty_entry_window_fails_closed) {
    static const struct { uint32_t insn; const char *name; } forms[] = {
        { ADRP,   "adrp" }, { ADR0,   "adr" },  { LDRLIT, "ldr-literal" },
        { B0,     "b" },    { BL0,    "bl" },   { CBZ0,   "cbz" },
        { CBNZ0,  "cbnz" }, { TBZ0,   "tbz" },  { TBNZ0,  "tbnz" },
        { BCOND0, "b.cond" },
    };
    for (size_t f = 0; f < sizeof(forms) / sizeof(forms[0]); f++) {
        for (int at = 0; at < 4; at++) {
            uint32_t prologue[16];
            for (int i = 0; i < 16; i++) prologue[i] = NOP;
            prologue[0] = STP_PRE_SP;          /* realistic frame push first ... */
            prologue[at] = forms[f].insn;      /* ... then the dirty instruction */
            ARM64PrologueAnalysis a;
            ASSERT_TRUE(arm64_analyze_prologue(prologue, 16, &a));
            if (a.safe_hook_offset != -1) {
                printf("    %s at index %d -> offset %d (expected -1)\n",
                       forms[f].name, at, a.safe_hook_offset);
            }
            ASSERT_EQ(a.safe_hook_offset, -1);
        }
    }
}

/* The dirty instruction just PAST the window (index 4) does not taint it:
 * the patch copies four instructions at most, so offset 0 stays correct. */
TEST(pc_relative_after_window_keeps_offset_zero) {
    uint32_t prologue[16];
    for (int i = 0; i < 16; i++) prologue[i] = NOP;
    prologue[0] = STP_PRE_SP;
    prologue[4] = ADRP;
    prologue[5] = LDR0;
    ARM64PrologueAnalysis a;
    ASSERT_TRUE(arm64_analyze_prologue(prologue, 16, &a));
    ASSERT_EQ(a.safe_hook_offset, 0);
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
    RUN_TEST(dirty_entry_window_fails_closed);
    RUN_TEST(pc_relative_after_window_keeps_offset_zero);
    RUN_TEST(alloc_near_lands_within_branch_reach);
}
