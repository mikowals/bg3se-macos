/*
 * Tier 0 tests for the Osiris hook-ABI contract in src/osiris/osiris_types.h.
 *
 * ARM64 hook wrappers must PASS THROUGH the original function's x0. A wrapper
 * whose function-pointer typedef declares `void` cannot: the value the original
 * produced is discarded, and whatever the wrapper does afterwards (logging,
 * Lua dispatch, context restore) decides what the engine reads as the result.
 *
 * COsiris::Event and COsiris::InitGame both return a value in x0 in the shipped
 * libOsiris.dylib; the C++ symbol cannot tell you that, because Itanium mangling
 * omits the return type, so nothing in the build catches the mismatch.
 *
 * These tests pin the typedef itself, which is the only part of the contract
 * that is observable offline.
 */

#include "test_harness.h"
#include "osiris_types.h"

/* The declared return type of each hookable Osiris entry point. */
typedef __typeof__(((OsiEventFn)0)(0, 0, 0))        OsiEventRet;
typedef __typeof__(((InternalQueryFn)0)(0, 0))      InternalQueryRet;
typedef __typeof__(((InternalCallFn)0)(0, 0))       InternalCallRet;

/* ── COsiris::Event ──────────────────────────────────────────────── */

TEST(osi_event_fn_is_not_void) {
    /* void here means a hook wrapper structurally cannot forward x0. */
    ASSERT_FALSE(__builtin_types_compatible_p(OsiEventRet, void));
}

TEST(osi_event_fn_return_is_register_width) {
    /* uint64_t is the shape that forwards raw x0 regardless of the
     * true C++ type (bool / int / pointer). */
    ASSERT_FALSE(__builtin_types_compatible_p(OsiEventRet, void));
    ASSERT_TRUE(__builtin_types_compatible_p(OsiEventRet, uint64_t));
}

/* ── Positive controls: these already return values in both trees ─── */

TEST(internal_query_fn_returns_int) {
    ASSERT_TRUE(__builtin_types_compatible_p(InternalQueryRet, int));
}

TEST(internal_call_fn_returns_int) {
    ASSERT_TRUE(__builtin_types_compatible_p(InternalCallRet, int));
}

void register_osi_hook_abi_tests(void) {
    printf("[osi_hook_abi]\n");
    RUN_TEST(osi_event_fn_is_not_void);
    RUN_TEST(osi_event_fn_return_is_register_width);
    RUN_TEST(internal_query_fn_returns_int);
    RUN_TEST(internal_call_fn_returns_int);
}
