---
title: "chore: PR merge + issue resolution plan vs v0.43.0 (main 01b6b91)"
type: chore
status: completed
date: 2026-09-14
---

# PR & Issue Triage vs v0.43.0 (main `01b6b91`)

**Project:** /Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos
**Repo plan copy (Phase 0):** `docs/plans/2026-09-14-001-chore-pr-issue-triage-v0430-plan.md`
**Precedent:** `docs/plans/2026-07-28-003-chore-pr-issue-triage-v0380-plan.md` (July pass, same shape)
**Review artifacts:** `scratchpad/pr-review/out-{pr101,pr103,pr91,issues-a,issues-b}.md` (Codex gpt-5.6-sol,
high reasoning, read-only) — copy the five reports into `docs/reviews/2026-09-14-codex/` in Phase 0.

## Context

Main has not moved since `01b6b91` (2026-08-10). The offline 7398727 migration landed; Phase 5 (live
session) and Phase 6 (deferred probes) of `docs/plans/2026-08-04-001-feat-offset-remigration-7398727-plan.md`
never ran. Three things follow from that:

- **No release since v0.39.0 (2026-04-24).** Main is v0.43.0 in source, targeting the current Steam
  build 4.1.1.7398727, but no user who cannot build from source has had a working artifact for five
  months. That vacuum, not any defect reproduced on main, is the root cause of issue #102: a fork
  (mageweaver, v0.44.1 → v0.47.5, daily releases since 2026-08-29) captured the user base, and a second
  fork (marcus-sa, 317 commits ahead of main) is producing fixes in isolation. marcus-sa closed their
  own PR #93 on 2026-09-01, the day the fork branch started.
- **PersistentVars is broken on main.** Every mod's saved variables are written as the four bytes
  `null` (PR #101 defect 1). This is the one main-side defect that would hit every SE mod user.
- **Three open PRs, all from mikowals.** #103 and #101 are based exactly on `01b6b91`, MERGEABLE,
  CI green. #91 is CONFLICTING, 47 behind, and was integrated via #95 on 2026-07-29.

Decisions taken with Tom (2026-09-14): integrate #101 ourselves with credit (July posture); cut
v0.44.0 after Phase 5 live verification; close epics #42/#35/#8, keep #70 narrowed.

## Established facts

### PR #101 — five defects, all confirmed live on main
| # | Defect | Pre-patch evidence | Codex verdict on the fix |
|---|--------|--------------------|--------------------------|
| 1 | PersistentVars save as `null` | `src/lua/lua_persistentvars.c:354-355` (`lua_gettop` after `luaL_buffinit`; Lua pushes the placeholder at `lib/lua/src/lauxlib.c:633-639`) | Correct, minimal, stack-balanced. No other `luaL_buffinit`+top-derived index in `src/lua/`. |
| 2 | `Ext.Json.Stringify(proxy)` → `"null"` | `src/lua/lua_json.c:265-293`, no `LUA_TUSERDATA` case | **Regressive.** `json_materialize` clones every table, caps at depth 32 and stores `nil` (deletes fields; old serializer allowed depth 200), has no visited set (branching cycle `t.a=t; t.b=t` expands exponentially). Production `_D` is the C implementation registered last (`src/injector/main.c:2562-2567`) and logs userdata as a pointer, so the PR's `_D()` claim is not met. |
| 3 | `Ext.Enums.X[n]` always nil | `src/enum/enum_ext.c:29-44` | Correct. Same `lua_isstring`-before-`lua_isinteger` ordering remains in `src/enum/enum_lua.c:61-78` and `src/enum/bitfield_lua.c:32-45`. |
| 4 | Hooks install at target+4 | `src/hooks/arm64_decode.c:355-367` | Analyzer change is sound (offset 0 when the 16-byte window is clean). But the narrative is wrong: `ExecuteStatsFunctors::AttackTarget` uses plain `DobbyHook` (`src/stats/functor_hooks.c:310-312`, see `docs/bugs/wave2-functor-crash-analysis.md:240-270`). The only live consumer whose offset changes is the `GetFreeMessage` net hook (`src/network/net_hooks.c:489-515`, +4 → 0). `leading_adrp_skips_forward` codifies the pre-existing unsafe dirty-window path (trampoline copies PC-relative instructions unrelocated, `src/hooks/arm64_hook.c:308-340`). |
| 5 | Osiris wrappers discard x0 | `src/osiris/osiris_types.h:197`, `src/injector/main.c:3275-3347,3827-4079` | Correct. `uint64_t` carries raw AAPCS64 x0; no early return before the original call in `fake_Event`; null-original path returns 0. Stale "void" comment at `main.c:3830-3833`. |

Test accounting: 68 → 90 (22 added, 13 fail pre-fix → 77/90, 90/90 post). All five suites registered in
`tests/tier0/test_main.c`; `CMakeLists.txt:391,397` has a harmless duplicate `src/osiris` include.
`tests/tier0/test_persistentvars.c` sets `HOME` without restoring it and shells out to `mkdir`/`rm -rf`.

### PR #103 — 23 tests, no source changes (Codex: no critical issues)
- Registered correctly; 68 → 91; combined with #101 → 113. No file overlap with #101.
- Three comments overclaim: the `read_string` loop-bound mutation (`i < max_len`) is NOT an overflow
  (`src/core/safe_memory.c:160` rewrites the last byte with NUL); the GPU pre-check remains
  mutation-equivalent (kernel rejects the address anyway); `sub_id_carries_full_salt_and_index` never
  touches `pool_pack()` (`entity_events.c` is not linked into tier 0).
- Canaries are adjacent struct members; safe under current code, UB only under an overflowing mutant.
  Prefer one backing array with a smaller logical size.
- `(int)0xFFFFFFFF` is implementation-defined; write `-1`.
- Two genuine follow-ups surfaced: **subscription-handle ABA after uint16 salt rollover**
  (`src/entity/entity_events.c:161,216-223,242-248`; same shape in `src/entity/ecs_system_update.c:121,373-395`)
  and **NULL contracts for `mod_se_dir_from_pak_name` / `mod_entry_se_config_dir`** (`src/mod/mod_paths.c:10-29`).
- CI runs tier 0 on `macos-latest` (ARM64) only; the x86_64 slice is untested.

### PR #91 — fully integrated
All nine contributions LANDED: offset table + routed call sites `3368581` (authorship preserved),
tool/manifest/guide patch-equivalent cherry-pick `452fb9f`, hardened `d16a5dc`, extended to 7398727
`01b6b91`. StaticData exact-version gate + TypeContext fallback (`staticdata_manager.c:332,389,696`);
Template table-backed reads, no live hook (`template_manager.c:210,249,453`); MAP_JIT protect `11f9d6d`.

### Issues — where the July follow-ups actually landed
Path discovery (`scripts/find_bg3.sh`, `BG3SE_GAME_PATH`, `libraryfolders.vdf` in `tools/bg3se_harness/config.py:13`,
`src/core/version_detect.c:134`, `scripts/deploy.sh:8-15`), the `launch_bg3.sh` marker fix (`8aba568`),
`docs/troubleshooting.md:146` arm64e entry, and the mod-detection fallback (`src/mod/mod_loader.c:382,522`)
all shipped. The corresponding issues got maintainer replies on 2026-07-29 and were never closed.
`RequireFiles` (#100) is not a builtin in Norbyte's reference either (grep of `bg3se/` is empty): it is a
mod-side helper, so that half of #100 is a downstream symptom, not a missing global. MCM has a live
27/27 baseline (`docs/compat-reports/baseline/mcm.json`, v0.41). The fork's SpellMeta-stride claim
(0x60 vs main's 80, `src/entity/component_property.h:51`, `component_offsets.h:713`) is a plausible
real defect on main and goes on the Phase 5 probe list.

### Gates and the release blocker
Open on 7398727: `COMPONENT_OPS`, `ECS_SYSTEM_UPDATE`, `FUNCTOR_ADDRS`. Closed: `SAVEGAME_HOOK`,
`VALUELIST_INSERT` (both 7209685). Phase 5 has not run. **Phase 5 patches the game binary and launches
BG3 — it requires Tom's explicit go at that checkpoint, not implied by approving this plan.**

## Dispositions

### PRs
| PR | Disposition | How |
|----|-------------|-----|
| #91 | **Close as integrated** | Post the 91-word closing comment from `out-pr91.md`, crediting @mikowals. |
| #103 | **Merge first, then a lead follow-up commit** | Merge as-is (no source risk, CI green). Lead commit: correct the three overclaiming comments, single-backing-array canaries, `-1` literal. File the two follow-up issues (salt ABA, NULL contracts). Reply on the PR with the review summary. |
| #101 | **Integrate on `integration/pr-101` with credit** | Post the Codex review as a PR comment (verdict, per-defect table, findings 1-8) and state the integration path. Cherry-pick `e4d5615f` + `292742aa` (authorship preserved). Lead commit reworks defect 2 and the defect-4 narrative (below). Merge to main after offline gates; live-verify in Phase 5; close PR as integrated. |

### Issues
| Issue | Class | Action | When |
|-------|-------|--------|------|
| #94 hotbar SIGBUS | FIXED-ON-MAIN (reporter confirmed) | Close with comment | Phase 1 |
| #92 Hotfix 36 SIGBUS | FIXED-ON-MAIN (`e27bcd8`/`11f9d6d`, superseded by `01b6b91`) | Close | Phase 1 |
| #90 external drive | FIXED-ON-MAIN (`684120c`) | Close; cite `BG3SE_GAME_PATH` | Phase 1 |
| #86 alternate dir | FIXED-ON-MAIN (`684120c`, `deploy.sh` warns-and-skips) | Close | Phase 1 |
| #84 injection marker | FIXED-ON-MAIN (`8aba568`); harness `doctor/status/patch` is the supported path | Close | Phase 1 |
| #89 CTD | STALE (reporter resolved locally, no artifacts) | Close | Phase 1 |
| #82 arm64e | NEEDS-INFO (misdiagnosis; real cause unknown) | Request artifacts; close after 14 days silent | Phase 1 |
| #88 `<tuple>` | OPEN, M | Note: SDK auto-detect predates the report (`720d067`, 2026-03-31), so the July reply was wrong; `doctor` does not check the toolchain despite `docs/harness.md:56`. Fix: CMake configure-time ObjC++ probe (`<tuple>` + MetalKit) with sysroot diagnostics; `doctor` checks for `xcode-select -p`, `xcrun --show-sdk-path`, compiler, libc++. Request reporter's cache values. | Phase 6 |
| #100 RequireFiles + StaticData | OPEN, partial | `RequireFiles` is mod-side: ask for the defining mod file + full log. Main-side gap is real: `CharacterCreationAppearanceVisual` is not among the nine StaticData types (`src/staticdata/staticdata_manager.h:35`) and an unknown type is a hard Lua error (`src/lua/lua_staticdata.c:75,120,154`) that aborts the mod's `SessionLoaded` handler. Add the type (Windows class `eoc::CharacterCreationAppearanceVisualManager`, `GuidResources.h:702`) via manager discovery + layout + tests. | Phase 6 |
| #80 overlay console | OPEN, L, own plan | Status comment: queue/output fixed; input still `NSTextField` + key window (`overlay.m:299,742,852,898`), `overlay_is_visible` sync-dispatch deadlock (`overlay.m:936`), listen-only tap (`input_hooks.m:240,341`), overlay errors dropped for slot −1 (`console.c:602,665`). Socket console is the supported interface. | Phase 1 comment; rework deferred |
| #102 fork thread | Boundary + plan | Reply (below); keep open until v0.44.0 ships, then close pointing at the release | Phase 1 + Phase 7 |
| #99 MCM | WORKS-ON-MAIN (27/27 v0.41 baseline) | Comment now (load order, F10 on non-US keyboards); re-run `compat run mcm` in Phase 5 after #101; close then. Fix `docs/supported-mods.md:41` Nexus id 8901 → 9162. | Phase 1 + 5 |
| #98 BG3SX | NEEDS-VETTING | Keep open; add a `bg3sx` compat scenario (NPC-discovery + scene-start assertions) after release; leading main-side gap is DB/PROC listener hooks (`main.c:2285`, `lua_osiris.c:49`) | Phase 7 |
| #97 call for targets | Tracker | Update body: all 10 originals baselined, Transmog + MCM verified; BG3SX, BG3AF, Demon Hunter, Twin Blades queued | Phase 1 |
| #70 startup | Keep, narrowed | Comment: instrumentation + deferred init exist (`main.c:3136,4669`); remaining = cold/warm baselines, budget, ranked phases | Phase 1 |
| #42 debugger, #35 Noesis, #8 tech debt | Close | Comments per `out-issues-b.md` (parity-excluded in `docs/deferrals.md:91`; #8 superseded by ROADMAP + 544 tests; leak detection gets its own issue when scheduled) | Phase 1 |

## Execution Plan

### Phase 0 — Persist the record (no code)
- Copy this plan to `docs/plans/2026-09-14-001-chore-pr-issue-triage-v0430-plan.md`.
- Copy the five Codex reports into `docs/reviews/2026-09-14-codex/` (strip ANSI; keep only the
  `=== Response ===` sections of the two reviewer runs).

### Phase 1 — GitHub hygiene (comments + closes, no code)
Tom-voice, declarative, crediting reporters. Drafts exist in `out-issues-a.md` / `out-issues-b.md`;
rewrite the #102 reply so it does **not** point users at the stale v0.39.0 release:

> The gap is ours: no release since April while main moved to 7398727. Plan: land #101 (PersistentVars,
> Enums, Osiris return values) and #103, run the live verification on the current Steam build, ship
> v0.44.0 with a zip. Fork users: report fork failures on the fork with exact versions. @marcus-sa,
> @mageweaver: focused PRs with a reproducer and a tier-0 or compat test land fast — resource-bank
> index fix, Vars identity, SpellMeta stride, DB/PROC listeners are the ones I want first. No bulk
> cherry-picks of a 317-commit branch.

Closes: #94 #92 #90 #86 #84 #89 #42 #35 #8. Comments: #82 #80 #99 #97 #70 #100 #88 #102. PR #91 close.
PR #103 and #101 review comments posted with the Codex findings summarized (not the raw dump).

### Phase 2 — Merge #103, follow-up commit
1. `gh pr merge 103 --merge` (keeps mikowals' commit).
2. Lead commit on main (`test(tier0): tighten #103 narratives and canary layout`): edit the three
   comments in `tests/tier0/test_safe_memory.c` and `test_entity_events.c`, switch the two canary structs
   in `test_mod_paths.c` and `test_safe_memory.c` to one backing array + smaller logical size, `-1` literal.
3. File issues: "Prevent subscription-handle ABA after generation rollover" (P2; both pools; text in
   `out-pr103.md`) and "Define NULL contracts for mod path helpers" (P3).
4. Gate: `cmake --build build --target bg3se_test_tier0 && ./build/bin/bg3se_test_tier0` → 91/91.

### Phase 3 — Integrate #101 on `integration/pr-101`
1. `git checkout -b integration/pr-101 main && git cherry-pick e4d5615f 292742aa`.
2. Lead commit `fix(json): serialize __pairs userdata in place with cycle guard (reworks #101 defect 2)`:
   - Delete `json_materialize`; add a `LUA_TUSERDATA` case to `json_sb_value` in `src/lua/lua_json.c`
     that, when `luaL_getmetafield(L, idx, "__pairs")` is non-nil, iterates and emits an object through
     the existing depth/indent machinery (reuse `JSON_MAX_DEPTH` 200, `lua_json.c:175-177,210-212`);
     opaque userdata stays `null`. No table cloning.
   - Add a visited set (light-userdata keys in a registry-anchored table, or a pointer stack for the
     active path) so a cycle emits `null` at the back-edge instead of recursing.
   - Route production `_D` (`src/injector/main.c:2541-2553`) through `lua_ext_json_stringify` for
     userdata with `__pairs` (lead-owned file, lead edits it).
   - Extend `tests/tier0/test_json_proxies.c`: linear cycle, branching cycle, depth 32/33 preserved,
     depth 201 truncated as before, opaque userdata → `null`, failing `__pairs` propagates cleanly.
3. Lead commit `test(arm64): attribute the +4 regression to GetFreeMessage, not AttackTarget`:
   rewrite the header comment and test names in `tests/tier0/test_arm64_prologue.c:4-12,25-36`; keep
   `leading_adrp_skips_forward` but comment that dirty-window relocation is a known gap; file issue
   "arm64_safe_hook: fail closed on dirty entry windows until relocation exists" (P2).
4. Lead commit `fix(enum): integer keys before string labels in enum_lua and bitfield_lua`
   (`src/enum/enum_lua.c:61-78`, `src/enum/bitfield_lua.c:32-45`) + tests for numeric bitwise operands
   and enum equality.
5. Lead commit `test(tier0): hermetic PersistentVars fixture` (save/restore `HOME`, `mkdir`/`unlink`/`rmdir`,
   no `system()`); remove the duplicate `src/osiris` include in `CMakeLists.txt`; fix the stale void
   comment at `main.c:3830-3833`.
6. Offline gate: build clean, `bg3se_test_tier0` ≥ 113 + new cases, `PYTHONPATH=tools pytest tests/harness/ -v`
   all green, `tests/harness/test_lua_state_ownership.py` unchanged.
7. Merge `integration/pr-101` to main with `--no-ff`; comment + close #101 as integrated with credit
   (commits keep mikowals' authorship; lead commits reference `Ported-from: #101`).
8. `docs/CHANGELOG.md` Unreleased: five fixes credited to @mikowals, JSON rework, enum follow-up, #103 tests.

### Phase 4 — Checkpoint (STOP for Tom)
Everything above is offline. Before continuing: `bg3se_harness status`, confirm the installed binary is
still 7398727 (`otool`/UUID `0C51CAED-…`) and the harness backup policy from the migration plan's
Wave 0 holds. **Ask Tom for the explicit go to patch and launch.**

### Phase 5 — Live verification on 7398727 (after Tom's go)
Follow `docs/plans/2026-08-04-001-…` Phase 5 verbatim, plus the items this pass added:
- `!identity` → one coherent PID/build; `!test` (tier 1); load a disposable save; `!test_ingame` (tier 2).
- **PersistentVars round trip**: set a marker in a test mod, save, reload, read back (defect 1 live).
- **Ext.Net alive** after the `GetFreeMessage` hook moves from +4 to 0: `Ext.Net.IsReady()`, one
  `PostMessageToServer` round trip (defect 4 live).
- **Osiris return forwarding**: new game reaches Running; `Ext.Debug.GetHookStatus()` shows both hooks
  installed; no story-init abort (defect 5 live).
- `Ext.Json.Stringify(entity.Health)` and `_D(entity.Health)` produce an object; `Ext.Enums.DamageType[7].Label == "Fire"`.
- `compat run mcm --launch` → 27/27; close #99.
- Component-proxy diagnosis (Transform.Translate, EntityUuid, GetEntitiesAroundPosition — Tasks #65/#67).
- **SpellMeta stride probe**: read `SpellContainer.Spells` on the host and check whether entry N+1 starts
  at +0x60 or +0x50; if 0x60, file a defect and fix in `component_offsets.h` before release.
- Record everything in `docs/parity-100/LIVE-VERIFICATION-2026-09-<dd>.md`.

### Phase 6 — Small fixes that ride the release
- #88: CMake ObjC++ compile probe + `doctor` toolchain checks; update `docs/harness.md:56`.
- #100: add `CharacterCreationAppearanceVisual` StaticData type (manager discovery, layout, serializer,
  test); reply about `RequireFiles` being mod-side.
- `docs/supported-mods.md:41` Nexus id fix.

### Phase 7 — Release v0.44.0 and close the loop
- Bump `src/core/version.h` → 0.44.0; CHANGELOG, README, ROADMAP, CLAUDE.md version lines (per
  `agent_docs/development.md` checklist). Parity % stays 94.8% unless Phase 5 changed the matrix.
- `gh release create v0.44.0` with the universal dylib zip + harness install one-liner + "verified on
  BG3 4.1.1.7398727" + credits (mikowals #101/#103; marcus-sa and mageweaver for the fork findings).
- Reply on #102 with the release link; close #102. Update #97 with the new baselines.
- Post-release queue (own issues, not this plan): #98 BG3SX scenario, #80 overlay rework, salt-ABA,
  dirty-window fail-closed, DB/PROC listener hooks, fork-claim triage list from `out-issues-b.md`.

## Verification
- Offline (Phases 2-3): `cmake --build build`, `./build/bin/bg3se_test_tier0` (91 after Phase 2,
  ≥ 113 after Phase 3), `PYTHONPATH=tools pytest tests/harness/ -v` (≥ 288 passed), CI green on main.
- Live (Phase 5): tier 1 + tier 2 green on one PID; the seven bullets above each have value-level
  evidence in the live-verification doc.
- GitHub: zero open PRs; every open issue has a 2026-09 maintainer comment; #102 closed against a
  release URL.

## Ordering & risk
Phases 0-3 are same-session and offline. Phase 4 is a hard stop. Never edit `src/injector/main.c`,
`src/lua/lua_ext.c`, or `CMakeLists.txt` in a subagent; never stage `lib/Dobby` or `tools/vendor/insert_dylib`;
no attribution trailers on commits. The biggest technical risk is the JSON rework (lead-written, must
not change `Ext.Json.Stringify` output for plain tables — add a golden test on an existing fixture
before touching the serializer). The biggest social risk is #102: the reply must own the release gap
and name concrete upstream asks, not litigate the fork.
