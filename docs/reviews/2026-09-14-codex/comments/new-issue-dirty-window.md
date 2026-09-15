Surfaced by the #101 review (`docs/reviews/2026-09-14-codex/pr-101-review.md`, finding on `leading_adrp_skips_forward`).

## Defect

`arm64_analyze_prologue()` (`src/hooks/arm64_decode.c`) returns a nonzero `safe_hook_offset` when the 16-byte entry window holds a PC-relative instruction, so `arm64_safe_hook()` installs past it. The trampoline then copies the skipped instructions unrelocated (`src/hooks/arm64_hook.c:308-340`). That is only correct when nothing in the skipped range is PC-relative and re-executed from the trampoline. A leading `adrp` in the skipped range computes the wrong page from the trampoline's PC.

The #101 analyzer fix (prefer offset 0 on a clean window) narrows when this path runs but does not make it safe. `tests/tier0/test_arm64_prologue.c::leading_adrp_skips_forward` pins the current behavior and is annotated as a known gap.

## Fix

Until relocation exists, `arm64_safe_hook()` fails closed on a dirty entry window: log the target and the offending instruction, return NULL, and let the caller degrade (the net hook already treats a failed install as "net disabled"). Relocation of `adrp`/`adr`/`b`/`bl`/literal `ldr` into the trampoline is the real fix and belongs in its own change with instruction-level tests.

Live consumers of `arm64_safe_hook()`: `src/network/net_hooks.c` (GetFreeMessage), `src/staticdata/staticdata_manager.c` (FeatManager, exact-version gated), `src/template/template_manager.c` (installer present, no live hook).

Priority: P2.
