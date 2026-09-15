# Verdict: REQUEST CHANGES

The core defects are real, but the JSON fix introduces new silent data loss and potentially exponential cycle expansion, while the ARM64 tests endorse a nonzero-offset strategy that remains unsafe and does not cover the claimed `AttackTarget` path.

## Per-defect assessment

| Defect | Claim confirmed? | Fix correct? | Residual risk | Notes |
|---|---|---:|---:|---|
| 1. PersistentVars serialized as `null` | **Yes.** `src/lua/lua_persistentvars.c:346,354-355`; Lua pushes the buffer placeholder at `lib/lua/src/lauxlib.c:633-639`. | **Yes** | Low | Capturing `pv_idx` before `luaL_buffinit` is correct and stack-balanced. |
| 2. Userdata proxies stringify as `null` | **Yes.** `src/lua/lua_json.c:265-293` has no userdata case. | **No; partial and regressive** | High | Proxy values become visible, but ordinary deep tables lose data, cycles can expand exponentially, and production `_D()` still bypasses this path. |
| 3. Numeric enum indexing is shadowed | **Yes.** `src/enum/enum_ext.c:29-44` tests `lua_isstring` before `lua_isinteger`. | **Yes, but incomplete** | Medium | The accessor is fixed and `"7"` remains a string label, but the same ordering defect remains in two related helpers. |
| 4. Clean entry window unnecessarily gets `+4` | **Partially.** The analyzer defect exists at `src/hooks/arm64_decode.c:355-367`; the claimed `AttackTarget` live path does not, because it uses direct `DobbyHook` at `src/stats/functor_hooks.c:310-312`. | **Partial** | High | Offset zero is correct for clean windows, but dirty-window nonzero offsets remain unsafe without relocation. Tests currently endorse that unsafe behavior. |
| 5. Osiris wrappers fail to preserve `x0` | **Yes.** Void typedef at `src/osiris/osiris_types.h:197`; trailing wrapper work at `src/injector/main.c:3275-3347,3827-4071`. | **Yes** | Medium | `uint64_t` preserves the raw AAPCS64 `x0` value. Wrapper-level tests remain insufficient for this lead-owned runtime change. |

## Findings

### 1. Critical: JSON materialization introduces silent depth-33 data loss and exponential cycle expansion

At PR head, `json_materialize` uses a depth cap of 32 and pushes `nil` once `depth > JSON_MAX_MATERIALIZE_DEPTH` at `src/lua/lua_json.c:328,333-335`. Its caller then stores that value with `lua_rawset` at `src/lua/lua_json.c:378-382`; storing `nil` deletes the field.

This regresses ordinary tables that the existing serializer accepts until depth 200 at `src/lua/lua_json.c:175-177,210-212`. Values at depth 33 disappear instead of becoming JSON `null`. Array elements can become holes and may change the container’s serialized shape.

There is also no visited-object set at `src/lua/lua_json.c:329-389`. A simple self-reference expands repeatedly until the cap; a branching cycle such as `t.a=t; t.b=t` can create exponentially many materialized tables before reaching it.

Other edge-case conclusions:

- Userdata without `__pairs` is copied unchanged at `src/lua/lua_json.c:338-342` and subsequently serializes as `null`. That is acceptable backward-compatible behavior.
- Errors from `__pairs` or its iterator propagate through the unprotected `lua_call` operations at `src/lua/lua_json.c:345-346,351-354`. Materialization runs before `luaL_buffinit` at `src/lua/lua_json.c:392-397`, so there is no C buffer to leak; Lua unwinds the temporary stack values.
- Every ordinary table is duplicated before serialization at `src/lua/lua_json.c:365-384`. Large component trees now require two full traversals and memory proportional to the entire materialized graph.

Suggestion: track active table/userdata identities and emit a JSON `null` sentinel on cycles. Keep the materialization limit consistent with `JSON_MAX_DEPTH`, and do not represent truncation as Lua `nil`. Prefer materializing only proxy-backed userdata rather than cloning all ordinary tables. Add linear-cycle, branching-cycle, depth-32/33, depth-200/201, opaque-userdata, failing-`__pairs`, failing-iterator, and large-tree tests.

### 2. Critical: the ARM64 fix leaves dirty-window hooks unsafe

The trampoline copies skipped instructions verbatim at `src/hooks/arm64_hook.c:308-340`. Consequently, a nonzero offset can both:

1. Run the skipped entry instructions before reaching the installed hook.
2. Run them again through the returned original trampoline.
3. Copy any PC-relative instruction without relocation.

The new test expects a leading `ADRP` to select an offset of at least eight bytes at `tests/tier0/test_arm64_prologue.c:48-61`. That expectation codifies unsafe behavior: the copied `ADRP` executes at a different PC, while preceding instructions are duplicated.

An `ADRP` at instruction three remains unsafe as well. The analyzer can select `+4` after instruction one, then place a 16-byte patch over the later `ADRP`.

The new entry-window check also relies on `arm64_decode_instruction(...).is_pc_relative`, but the raw classifier recognizes ADR and literal-load forms at `src/hooks/arm64_decode.h:193-214` that are not fully represented by the decoder cases at `src/hooks/arm64_decode.c:63-110,140-222`. Such an entry can be incorrectly declared clean.

Suggestion: return offset zero only when the actual patch window is clean. If it is dirty, fail closed until the trampoline supports relocation. Use `arm64_is_pc_relative()` for the window decision and add tests for `ADRP`/`ADR`/literal-load instructions at indices zero through three. Change the current leading-`ADRP` test to expect rejection.

### 3. High: the ARM64 regression test does not exercise the claimed `AttackTarget` path

`ExecuteStatsFunctors::AttackTarget` is installed with plain `DobbyHook` at `src/stats/functor_hooks.c:310-312`. It never calls `arm64_analyze_prologue` or `arm64_hook_install`.

The repository’s existing analysis says the same thing at `docs/bugs/wave2-functor-crash-analysis.md:240-270`: the functor installer bypasses the custom hook module.

The current hook-site audit is:

- Actual custom consumer affected now: `GetFreeMessage` at `src/network/net_hooks.c:489-515`, which changes from `+4` to zero on the frozen prologue.
- Conditional custom consumer: Feat hook at `src/staticdata/staticdata_manager.c:1014-1037`; current frozen entries have a clean first window and therefore take its Dobby fallback.
- Explicit zero-offset consumer: savegame hook at `src/game/savegame_hook.c:157-160`.
- Unused custom helper: `src/templates/template_manager.c:160-176`; the active installation path at `src/templates/template_manager.c:256-285` does not call it.
- Direct Dobby sites, unaffected by this patch: Osiris hooks at `src/injector/main.c:4252,4265,4278,4291`; Bink at `src/video/video_skip.c:100`; functors at `src/stats/functor_hooks.c:312,321,330,339,348,357,366,375,384,393`; static-data hooks at `src/staticdata/staticdata_manager.c:717,1037`.

Suggestion: rename and rewrite `tests/tier0/test_arm64_prologue.c:4-12,25-36` as a regression test for the custom `GetFreeMessage` hook-selection logic. Do not describe it as an `AttackTarget` crash fix.

### 4. High: production `_D(componentProxy)` still does not reach the fixed serializer

The Lua `_D` helper explicitly sends userdata to `tostring` at `src/lua/lua_ext.c:1416-1424`.

More importantly, initialization registers the Lua helpers and then overwrites `_D` with the C implementation:

- Helper registration: `src/injector/main.c:1119-1120,2845-2858`.
- Later C `_D` registration: `src/injector/main.c:2562-2567`.
- The C implementation only invokes JSON serialization for tables at `src/injector/main.c:2541-2548`; userdata is logged as a pointer at `src/injector/main.c:2551-2553`.

No top-level `Ext.Dump` is registered by `lua_ext_register_basic` at `src/lua/lua_ext.c:500-523`.

Suggestion: either remove the userdata short-circuit and prevent the later `_D` overwrite, or teach the final C `_D` implementation to call `lua_ext_json_stringify` for proxy userdata. Add an integration test using the final production registration order.

### 5. Medium: the same enum type-ordering bug remains in related APIs

The PR correctly makes integer lookup precede exact-string lookup at `src/enum/enum_ext.c:29-58`. Therefore `Ext.Enums.DamageType[7]` works and `Ext.Enums.DamageType["7"]` remains a label lookup that returns nil, as tested at `tests/tier0/test_enum_ext.c:195-204`.

However, the same ordering remains at:

- `src/enum/enum_lua.c:61-78`, where the string branch shadows the intended integer equality branch.
- `src/enum/bitfield_lua.c:32-45`, where integer operands are swallowed by the string branch before bitwise operators use the helper at `src/enum/bitfield_lua.c:129-165`.

There is no `generated_enums.c` in this repository; the relevant implementations are under `src/enum/`.

Suggestion: reorder those branches and tighten them to `lua_type(...) == LUA_TSTRING`. Add direct numeric bitwise-operand and enum-equality metamethod tests.

### 6. Medium: ABI fix is correct, but tests do not verify the wrappers

AAPCS64 returns scalar values through `x0`; `uint64_t` is a suitable raw carrier for both observed return paths. Values originally produced through `w0` remain valid because AArch64 writes to `w0` zero the upper half.

The patched `fake_Event` has no early return before the original call at `src/injector/main.c:4067-4070`; its gate path only jumps to `after_tick` at `src/injector/main.c:3844-3846`. It returns the captured value at `src/injector/main.c:4079`. The null-original path deterministically returns zero. `fake_InitGame` behaves equivalently at `src/injector/main.c:3275-3347`.

No other `orig_Event` or `orig_InitGame` cast retains a void signature. However, `tests/tier0/test_osi_hook_abi.c:20-37` primarily pins `OsiEventFn`; it would not detect a wrapper that failed to capture or return the original value. The stale comment at `src/injector/main.c:3830-3833` still describes Event as void.

Suggestion: add sentinel-return tests around extracted wrapper helpers or compile-time assertions for both fake wrapper signatures, and update the stale comment.

### 7. Low: PersistentVars fix is complete and balanced

The pre-patch value is at the top of the Lua stack at `src/lua/lua_persistentvars.c:346`. `luaL_buffinit` then pushes a placeholder at `src/lua/lua_persistentvars.c:354`, making the subsequent `lua_gettop` at line 355 refer to that placeholder. The default serializer emits `null` at `src/lua/lua_json.c:292`.

The PR captures the PersistentVars index before buffer initialization at PR-head `src/lua/lua_persistentvars.c:352-361`. `luaL_pushresult` replaces/removes the buffer placeholder, and the existing pop at `src/lua/lua_persistentvars.c:389` removes the JSON result, PersistentVars value, and mod table. The iteration key and final Mods cleanup remain correct at `src/lua/lua_persistentvars.c:389-392`.

No other `luaL_buffinit` followed by a top-derived value index exists under `src/lua/`.

### 8. Low: the PersistentVars tests are offline-safe but not hermetic

`tests/tier0/test_persistentvars.c:29-42` creates a temporary HOME and invokes shell `mkdir`; cleanup uses shell `rm -rf` at `tests/tier0/test_persistentvars.c:127-130`.

The test does not restore the prior `HOME`, failure longjmps bypass cleanup, and an apostrophe in `TMPDIR` can break the constructed shell command. It requires no game or socket, but it can leave process state and temporary files behind.

Suggestion: save and restore `HOME`, and use `mkdir`, `unlink`, and `rmdir` directly with cleanup registered before assertions.

## Test evaluation

Static registration and counts are consistent:

- Base: 68 Tier-0 `TEST` definitions.
- PR #101: 90, adding 22.
- Expected pre-fix failures: 13, producing the claimed 77/90.
- Post-fix expectation: 90/90.
- All five new suites are declared and invoked at `tests/tier0/test_main.c:19-39`.
- All five sources and required production sources are included at PR-head `CMakeLists.txt:357-400`.
- `CMakeLists.txt:391,397` contains a harmless duplicate `src/osiris` include directory.

The tests are game- and socket-independent. The ARM64 allocation test at `tests/tier0/test_arm64_prologue.c:69-78` assumes Darwin `MAP_JIT`, and the ABI tests use Clang extensions; both match the project’s macOS/Clang target.

I did not execute the build because this review was explicitly read-only and the test/build process creates artifacts. The reported Tier 0 and Tier H CI passes establish that the patched tree compiles and executes, but they do not cover the regressions above.

PR #103 changes only:

- `tests/tier0/test_entity_events.c`
- `tests/tier0/test_mod_paths.c`
- `tests/tier0/test_pattern_scan.c`
- `tests/tier0/test_safe_memory.c`

It does not overlap PR #101’s files and does not modify `tests/tier0/test_main.c` or `CMakeLists.txt`, so there is no registration conflict. Its 23 tests would produce 113 Tier-0 tests when combined with PR #101.

## Lead-owned file changes

- `src/injector/main.c:3275-3347,3827-4079` — **Medium/high runtime risk.** The ABI change is technically correct, but it touches live Osiris hooks and lacks wrapper-level return-forwarding tests.
- `CMakeLists.txt:357-400` — **Low risk.** Test-only registration is correct; remove the duplicate `src/osiris` include.
- `src/lua/lua_ext.c` — **Unchanged**, but the claimed `_D()` behavior cannot be completed without changing this lead-owned registration/helper path or the lead-owned C `_D` implementation. Treat that follow-up as medium risk because registration order determines the public behavior.

## Required and suggested commits

Required before merge:

1. Replace JSON depth-only materialization with cycle-aware behavior that preserves ordinary-table semantics; add cycle, depth, error, and performance-boundary tests.
2. Make dirty ARM64 entry windows fail closed unless proper PC-relative relocation is implemented; update the `ADRP` expectations and test all four entry-window positions.
3. Remove the unsupported `AttackTarget` attribution from the ARM64 tests.
4. Route the final production `_D()` implementation through proxy-aware serialization, or narrow the PR claim and add the missing behavior in the same series.

Suitable immediately after merge once the blockers are addressed:

5. Fix the remaining enum/bitfield `lua_isstring` ordering sites and add numeric operand tests.
6. Add real wrapper return-forwarding tests for `fake_InitGame` and `fake_Event`; correct the stale void-return comment.
7. Make the PersistentVars fixture hermetic and remove the duplicate CMake include entry.
tokens used
351,258
