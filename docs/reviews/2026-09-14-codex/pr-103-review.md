# PR #103 Review

**Verdict: REQUEST CHANGES — most additions are useful and correctly registered, but several tests do not catch the mutations their comments claim, and the canary fixtures make mutation runs formally undefined.**

| Test file | Tests added | Targets real invariant? | Fragility risk | Notes |
|---|---:|---|---|---|
| `test_safe_memory.c` | 6 | Partial | Medium | Gap containment is genuinely covered. GPU helper boundaries are pinned, but deleting the `safe_memory_read()` guard remains undetected. The claimed string-loop mutation still passes. |
| `test_mod_paths.c` | 6 | Yes | Medium under mutation | Boundary, suffix, case, and exact-path behavior match `mod_paths.h`. The `>=` → `>` mutation is detected by the return assertion, but the struct canary does not make the mutant’s overflow defined. |
| `test_pattern_scan.c` | 8 | Mostly | Low | Rejection, case-insensitive hex, null handling, and first-match behavior are real contracts. Wildcard-byte zeroing is an internal representation invariant rather than scanning behavior. |
| `test_entity_events.c` | 3 | Partial | Low | Tag values match upstream BG3SE. The sign-extension test is useful. The “full salt and index” test does not exercise `pool_pack()` and substantially duplicates existing macro coverage. |

The 23 additions are all registered. Post-patch totals are 23 safe-memory, 18 pattern-scan, 9 entity-event, 20 mod-path, plus the unchanged 21 tests: **91 total**. The existing registrars are already called from `tests/tier0/test_main.c:23-29`.

## Critical Issues

None.

## Improvements

1. **[Medium] `tests/tier0/test_safe_memory.c:169-210` — the stated string-loop mutation is not detected.**

   Changing `i < max_len - 1` to `i < max_len` does not write beyond the destination. It writes `buffer[max_len - 1]`, after which `src/core/safe_memory.c:160` overwrites that same byte with `'\0'`. All three new tests still pass:

   - The ordinary copy stops at the source NUL.
   - The truncation result remains `"ABC"`.
   - The exact-fit case reads its NUL into the last valid byte.

   Suggestion: replace the inaccurate mutation narrative with the mutation described in the commit body—corrupting or omitting copied bytes—which `read_string_copies_and_terminates` genuinely detects. Do not characterize the loop-bound change as an overflow.

2. **[Medium] `tests/tier0/test_safe_memory.c:113-128` — the GPU pre-check remains mutation-equivalent.**

   `gpu_region_pins_documented_range` validates `safe_memory_is_gpu_region()`, but deleting the call at `src/core/safe_memory.c:75-78` still leaves all 91 tests green because `mach_vm_read_overwrite()` rejects the unmapped address independently. The PR comment correctly admits this, but the PR should not be described as covering the pre-check itself.

   Suggestion: track call-path coverage separately, likely through an injectable/wrapped Mach read function. The exact constants are a valid current ARM64 safety contract, although they remain observed hardware/OS behavior rather than a Mach API guarantee.

3. **[Medium] `tests/tier0/test_entity_events.c:73-83` — this does not test `pool_pack()`.**

   The test constructs `packed` locally, passes it through `MAKE_SUB_ID`, and decomposes the same constant. `entity_events.c` is not linked into Tier 0, so mutations to `pool_pack()` at `src/entity/entity_events.c:251-253` are invisible. Existing `sub_id_index_extraction` and `sub_id_max_index` already verify preservation of all 32 low bits.

   Suggestion: either describe this honestly as redundant macro-layout coverage, remove it, or expose/factor the pool packing logic so the implementation itself can be tested.

4. **[Low] `tests/tier0/test_mod_paths.c:107-137` and `tests/tier0/test_safe_memory.c:172-210` — canaries are adjacent struct members, not part of the same array object.**

   Current production code stays within bounds, so the merged tests themselves are safe. Under an intentionally overflowing mutant, however, `s.dir[4]` or `s.buf[N]` dereferences one past a member array and is undefined behavior even if the next struct member occupies that address.

   Suggestion: use one backing array, pass its beginning with a smaller logical `dir_size`/`max_len`, and inspect the remaining bytes as canaries. That keeps the mutant’s physical write within the owning array and makes mutation results deterministic.

5. **[Low] `.github/workflows/test-offline.yml:7` / `CMakeLists.txt:384-387` — Tier 0 does not explicitly cover x86_64.**

   The test target is forced to the host architecture, and CI has one `macos-latest` job with no Rosetta or Intel matrix entry. Today `macos-latest` is ARM64 according to the [official GitHub runner-image table](https://github.com/actions/runner-images/blob/main/README.md#available-images). Thus the universal dylib’s x86_64 slice is not tested.

   The new Mach allocation/gap test should behave on either architecture, and the GPU predicate test does not inspect actual mappings, so this is not an immediate blocker. Add `macos-15-intel` if x86_64 remains supported behavior.

## Minor/Style

1. **`tests/tier0/test_pattern_scan.c:133-143`** — wildcard-byte zeroing is deterministic hygiene, not a matching invariant: `find_pattern()` ignores `bytes[i]` whenever `mask[i] == 0` at `src/osiris/pattern_scan.c:113-116`. If retained, document canonical initialization as part of `BytePattern`; otherwise this pins an incidental representation. Removing the initialization would also make the test nondeterministic because allocator memory can already contain zeroes.

2. **`tests/tier0/test_pattern_scan.c:105-108`** — `sscanf()` does not leave arbitrary garbage here; `byte` is initialized to zero at `src/osiris/pattern_scan.c:81`. An “accept invalid tokens” mutation would produce zero bytes.

3. **`tests/tier0/test_entity_events.c:87-91`** — `(int)0xFFFFFFFF` is implementation-defined. Supported Apple Clang targets produce `-1`, but `int index = -1;` expresses the sign-extension case portably.

4. The assertion helpers, snake_case naming, `TEST`/`RUN_TEST` registration, and section separators match the surrounding Tier-0 conventions. The long audit comments and hard-coded “68 tests” are more verbose and more quickly stale than neighboring tests.

## Uncovered Defects

### Subscription salt rollover: genuine; file a follow-up

This is a real stale-handle/ABA bug:

- `src/entity/entity_events.c:161` stores the generator as `uint16_t`.
- `pool_alloc()` assigns and increments it at `src/entity/entity_events.c:216-223`.
- `pool_validate()` accepts a handle solely when its 16-bit index and salt match an active slot at `src/entity/entity_events.c:242-248`.

After salt reuse, an old handle for the same slot can validate against a new subscription and unsubscribe or address the wrong callback. Component salts reach zero on allocation 65,536 and repeat `1` on allocation 65,537. The analogous system-subscription allocator at `src/entity/ecs_system_update.c:121,373-395` skips zero but repeats after 65,535 values.

Recommended follow-up issue:

> **Prevent subscription-handle ABA after generation rollover**
>
> Both component and ECS system subscription pools use a 16-bit generation in their 32-bit packed handles. After generation rollover, a stale Lua handle can validate against a reused active slot. Audit both allocators, use more of the available low 32 bits for generation if wire compatibility permits, or fail safely before a generation is reissued. Add allocator-level rollover tests through a small test seam or factored packing module.

Priority: **P1/P2**, depending on expected subscription churn. It is not C memory corruption, but it can act on the wrong live subscription.

### NULL path input: real crash, lower urgency

`mod_se_dir_from_pak_name(NULL, ...)` dereferences through `strrchr()` at `src/mod/mod_paths.c:10-12`. `mod_entry_se_config_dir(NULL, ...)` has the same class of problem at `src/mod/mod_paths.c:25-29`; both helpers can also write through a NULL output for otherwise valid input.

Current call sites pass non-NULL stack or PAK-entry strings, and the header does not explicitly promise NULL tolerance, so this is defensive hardening rather than an observed production path. It is still worth a follow-up:

> **Define and enforce NULL contracts for mod path helpers**
>
> Return `0` for NULL input/output pointers or explicitly document non-NULL preconditions. Add direct Tier-0 tests alongside the guards. The defect is not inherently unreachable from Tier 0 once the validation exists.

## PR #101 Merge Order

Merge the amended **#103 first**, then rebase or merge **#101** and rerun the combined suite.

There is no file-level overlap:

- #103 changes four existing test implementation files.
- #101 changes `tests/tier0/test_main.c`, `CMakeLists.txt`, adds five new test files, and changes unrelated production files.

The registration blocks therefore do not conflict. #101 adds 22 tests, so both PRs together should report **113 tests**. Either order should merge cleanly, but #103-first establishes the hardened existing-suite baseline before #101 expands the runner.

`git diff --check` is clean for both branches, and PR #103’s current GitHub checks are green. No local build was run because this review was required to remain strictly read-only.
tokens used
136,834
