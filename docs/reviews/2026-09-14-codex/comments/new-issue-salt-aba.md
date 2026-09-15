Surfaced by the #103 test pass (@mikowals) and the Codex review of that PR (`docs/reviews/2026-09-14-codex/pr-103-review.md`).

## Defect

Both subscription pools pack a 16-bit generation ("salt") into the low 32 bits of the handle they hand to Lua, and validate a handle solely by index + salt match against the active slot.

- `src/entity/entity_events.c:161` — `static uint16_t g_next_salt = 1;`, assigned and incremented in `pool_alloc()` (`:216-223`), checked in `pool_validate()` (`:242-248`). The salt wraps to 0 on allocation 65,536 and repeats 1 on 65,537.
- `src/entity/ecs_system_update.c:121,373-395` — same shape; skips zero but repeats after 65,535 values.

After rollover, a stale handle held by a mod for slot N validates against whatever subscription now occupies slot N. `Unsubscribe` then removes the wrong live callback, and any handle-addressed operation acts on the wrong subscriber. Not memory corruption, but a silent cross-mod misfire under high subscription churn (event-heavy mods, repeated session loads).

## Fix

- Use more of the low 32 bits for the generation where the wire format allows (the index needs far fewer than 16 bits at `MAX_SUBSCRIPTIONS`), or fail closed when a generation would be reissued while the slot is live.
- Apply the same change to both allocators.
- Add allocator-level rollover tests in tier 0 through a small test seam or by factoring the pack/validate pair into its own unit (the current pool code is not linked into `bg3se_test_tier0`; `tests/tier0/test_entity_events.c` pins only the `MAKE_SUB_ID` macros).

Priority: P2.
