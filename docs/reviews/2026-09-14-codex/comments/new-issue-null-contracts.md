Surfaced by the #103 test pass (@mikowals) and the Codex review of that PR (`docs/reviews/2026-09-14-codex/pr-103-review.md`).

## Defect

`mod_se_dir_from_pak_name(NULL, ...)` dereferences through `strrchr()` at `src/mod/mod_paths.c:10-12`. `mod_entry_se_config_dir(NULL, ...)` has the same shape at `src/mod/mod_paths.c:25-29`. Both helpers also write through a NULL output buffer for otherwise valid input. Every current call site passes non-NULL stack or PAK-entry strings, and the header promises nothing about NULL, so this is hardening rather than an observed crash.

## Fix

Either return `false` for NULL input or output pointers, or document non-NULL preconditions in `src/mod/mod_paths.h` and assert them. Add direct tier-0 cases for both helpers next to the existing boundary tests in `tests/tier0/test_mod_paths.c`.

Priority: P3.
