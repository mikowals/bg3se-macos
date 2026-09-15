=== Response ===
I’ll run this as a strictly read-only editorial and fact-check pass. I’m using the BG3SE macOS research skill because it governs repository-specific investigation; I’ll read Tom’s voice guide first as requested, then the skill instructions, drafts, plan, reports, and cited evidence.
The voice guide confirms the key pressure points for this pass: declarative claims, connected em dashes, clause stacking instead of staccato fragments, and factual/action closers. I’m inventorying the 21 drafts now, then I’ll trace every cited SHA, path, count, and disposition against the repository evidence.
The supplied reports support the technical dispositions, but one draft already contains a concrete path error: `issue-80.md` names `src/ui/overlay.m`, while the implementation and review evidence use `src/overlay/overlay.m`. I’m now checking the remaining SHAs and numeric claims directly, including baseline summaries rather than relying on the review prose.
The numeric checks are clean: all ten issue-97 baselines match their stated pass counts, MCM is 27/27, the StaticData enum contains nine types, and the documented four-tier total is 544. The two remaining factual corrections are both SHA-related: `e27bcd8` exists as an object but is not an ancestor of `main`, so the issue-92 and issue-94 drafts need the integrated main commit instead.
## Answer

### issue-100.md

- VERDICT: POST WITH EDITS
- FACTS: all confirmed.

```md
Two different things here.

`RequireFiles` is not an extender builtin—not on macOS, and not in Norbyte's Windows reference either. The mod's own bootstrap defines this helper, so a missing global means an earlier file in its load sequence failed. Post that definition and the full `latest.log`, and I can identify the failure.

The StaticData half is a real gap on main: only nine types are registered (`src/staticdata/staticdata_manager.h`) and `CharacterCreationAppearanceVisual` is not among them, so the lookup raises a Lua error that aborts the mod's `SessionLoaded` handler. Adding the type (`eoc::CharacterCreationAppearanceVisualManager` on the Windows side) through manager discovery, layout and tests is queued for v0.44.0. Keeping open.
```

### issue-102.md

- VERDICT: POST WITH EDITS
- FACTS: “v0.39.0 in April” is wrong—`docs/CHANGELOG.md:737` and the local tag date it to 2026-07-29. GitHub release history is UNVERIFIED because `gh release list` could not reach `api.github.com`; the revised wording uses the confirmed local tag history. The four requested fork fixes all appear in `issues-b-review.md`.

```md
Credit first: @mageweaver and @marcus-sa shipped fixes and builds while this repo went quiet, and the people in this thread got a working extender out of it. The release gap is ours—v0.39.0 remains the latest tag while main moved to 4.1.1.7398727 in source. This drove the "which build do I install" questions above.

What main does next, in order:

1. Land #101 (PersistentVars saved as `null`, `Ext.Enums.X[n]` always nil, net hook installed at target+4, Osiris return values discarded) and #103.
2. Run live verification on the current Steam build.
3. Ship v0.44.0 with a prebuilt zip, the harness install one-liner, and the verified game build in the release notes.

Until v0.44.0 is out, treat the fork releases as working builds, and report their failures to the corresponding repository with exact extender, manager, mod and game versions. This thread is not the place for those.

@marcus-sa, @mageweaver: focused PRs with a reproducer and a tier-0 or compat test land fast. I want the resource-bank index off-by-one, Vars table identity, SpellMeta stride, and DB/PROC listener fixes first, each backed by its own ARM64 evidence. A 317-commit branch will not be bulk-cherry-picked.

Leaving this open until the release link is posted here.
```

### issue-35.md

- VERDICT: POST AS-IS
- FACTS: all confirmed.

### issue-42.md

- VERDICT: POST AS-IS
- FACTS: all confirmed.

### issue-70.md

- VERDICT: POST WITH EDITS
- FACTS: all confirmed.

```md
Narrowing the scope. Startup instrumentation and staged initialization already exist, including timing breadcrumbs in `main.c` and delayed entity, stats and StaticData work. What remains: cold and warm launch baselines, a performance budget, subsystem costs ranked, and only changes with before/after numbers. Harness-test compilation and those delayed tasks are the first measurement targets. Keeping open with that scope.
```

### issue-8.md

- VERDICT: POST WITH EDITS
- FACTS: all confirmed, including the documented 544-test total (`docs/testing.md:54`, `ROADMAP.md:1461`).

```md
Closing—superseded by `ROADMAP.md` and focused subsystem issues. Pattern scanning, four test tiers (544 tests, tiers 0 and H in CI), the API and architecture docs, and the crash-resilience work (mmap ring buffer, Mach exception handler, `lua_gate`) have all landed. The two leftovers move to focused owners: performance benchmarking lives in #70, and memory-leak detection gets a dedicated issue when it is scheduled.
```

### issue-80.md

- VERDICT: POST WITH EDITS
- FACTS: `src/ui/overlay.m` does not exist. The implementation is `src/overlay/overlay.m` (`src/overlay/overlay.m:299`, `src/overlay/overlay.m:936`). All other claims confirmed.

```md
Main now drains the command queue, serializes Lua, preserves output order, and avoids the TextKit output-flood crash. The input path remains broken: an editable `NSTextField` makes the overlay the key window (`src/overlay/overlay.m`), `overlay_is_visible` deadlocks when invoked on the main queue because it synchronously dispatches there, the CGEvent tap is listen-only, and overlay errors are dropped for the console slot. The Mods and Entities tabs remain placeholders.

The fix moves input out of AppKit and into the existing ImGui backend. That work has its own plan and will not ride the v0.44.0 release. Until then, the socket console (`bg3se-console`, or `nc -U /tmp/bg3se.sock`) is the supported interface. Keeping this open as the tracking issue.
```

### issue-82.md

- VERDICT: POST WITH EDITS
- FACTS: all confirmed.

```md
The arm64e diagnosis is wrong. BG3 is plain arm64, and the extender is universal arm64 + x86_64—this project neither builds nor needs an arm64e Dobby slice. The actual failure remains unidentified. To find it I need the BG3 build, the exact extender commit, the failing command with its full output, `bg3se_harness doctor` and `status` JSON, `file` on both the BG3 executable and dylib, and `latest.log` or the newest `.ips` report. Those artifacts distinguish build, patch, signing, launch and runtime failures. Without them within 14 days, this closes.
```

### issue-84.md

- VERDICT: POST WITH EDITS
- FACTS: all confirmed.

```md
The marker bug is fixed in `8aba568`: the old launch script checked for a file the dylib never wrote, while your session log already proved injection had succeeded. Main now verifies that log directly. The supported path is the harness—`bg3se_harness doctor`, `status`, `patch`—with static `insert_dylib` patching. `DYLD_INSERT_LIBRARIES` is dead through Steam. The cosmetic PAKs you listed are loaded by the game via `modsettings.lsx`, not by the extender, so they were never affected by this. Closing.
```

### issue-86.md

- VERDICT: POST AS-IS
- FACTS: all confirmed.

### issue-88.md

- VERDICT: POST AS-IS
- FACTS: all confirmed.

### issue-89.md

- VERDICT: POST AS-IS
- FACTS: all confirmed.

### issue-90.md

- VERDICT: POST AS-IS
- FACTS: all confirmed.

### issue-92.md

- VERDICT: POST WITH EDITS
- FACTS: `e27bcd8` exists in the object database but is not on `main` (`git merge-base --is-ancestor e27bcd8 main` exits 1). The integrated StaticData safety work is in `3368581`; all other claims confirmed.

```md
Fixed on main. The three readable data sentinels can no longer authorize the stale StaticData and template code patches that crashed Hotfix 36: StaticData patch installers require the exact verified build (`3368581`), template capture is pointer-read-only, and functor patches use an independent ABI gate (`11f9d6d`). Main now targets 4.1.1.7398727, and an unknown build fails closed instead of patching blind. Closing—a v0.44.0 release with a prebuilt zip follows once live verification on the current build completes.
```

### issue-94.md

- VERDICT: POST WITH EDITS
- FACTS: `e27bcd8` is not on `main`; use integrated commit `3368581`. The remaining SHA and implementation claims are confirmed.

```md
Your retest confirmed the fix on main. StaticData code patches now require the exact verified game build (`3368581`), template capture is pointer-read-only with no code hooks, and functor patches use an independent ABI gate (`11f9d6d`) targeting 4.1.1.7398727 in `01b6b91`. The later session-init failure came from obsolete `DYLD_INSERT_LIBRARIES`, which is gone—`insert_dylib` through the harness is the supported route. Thanks for the deterministic repro and follow-through. Closing.
```

### issue-97.md

- VERDICT: POST AS-IS
- FACTS: all confirmed. The ten baseline summaries match all stated counts, and Transmog Enhanced Revamped is 20/20.

### issue-98.md

- VERDICT: POST WITH EDITS
- FACTS: all confirmed.

```md
BG3SX has not been vetted on main, so this stays open as a compat target. MCM is verified, not the blocker. The first main-side gap to test is story integration: native Osiris calls work, but Osiris database and PROC listeners are not live-verified, matching "NPC discovery or scene start never advances."

The acceptance test will be a `bg3sx` harness scenario asserting eligible-NPC discovery and scene startup. Post the exact BG3SX and BG3AF versions and a clean load order so the scenario matches what you run. Queued after v0.44.0.
```

### issue-99.md

- VERDICT: POST WITH EDITS
- FACTS: MCM’s 27/27 result and the 8901/9162 mismatch are confirmed. The ID is not yet fixed—`docs/supported-mods.md:41` still contains 8901—so the revised comment states the planned action.

```md
MCM works on main: the v0.41 compat baseline passes 27/27 assertions—viewport state, events, listeners, PersistentVars, Tick, `DB_Players` (`docs/compat-reports/baseline/mcm.json`). Two setup notes: put it first in the load order, and on keyboards without a usable `INSERT`, rebind the toggle to F10 in its settings JSON. Whether a mod manager lists it has nothing to do with extender compatibility.

I am rerunning the scenario after #101 lands, since that PR fixes PersistentVars serialization (saved as `null` today), and will close this against v0.44.0 with the fresh report. I will fix the wrong Nexus id in `docs/supported-mods.md` (8901 for 9162) in the same pass.
```

### pr-101.md

- VERDICT: POST WITH EDITS
- FACTS: The opening overstates defect 4 as fully correct; the review rates it partial because clean-window selection is fixed while dirty-window relocation remains unsafe (`docs/reviews/2026-09-14-codex/pr-101-review.md:12`). All other claims confirmed.

```md
All five defects reproduce on main. Fixes 1, 3 and 5 are correct, while 4 restores zero-offset hooks for clean windows but leaves dirty-window relocation unresolved. Thank you @mikowals—PersistentVars saving as `null` hits every SE mod user.

Review (Codex gpt-5.6-sol, read-only, full report in `docs/reviews/2026-09-14-codex/pr-101-review.md`):

| Defect | Verdict |
|---|---|
| 1 PersistentVars `null` (`lua_gettop` after `luaL_buffinit`) | Correct, minimal, stack-balanced. |
| 2 `Ext.Json.Stringify(proxy)` → `"null"` | Right diagnosis, wrong shape. `json_materialize` clones every table, caps at depth 32 and silently drops fields where the serializer allowed 200, and has no visited set—`t.a=t; t.b=t` expands exponentially. Production `_D` is the C implementation registered last in `main.c`, so the `_D()` claim is not met. |
| 3 `Ext.Enums.X[n]` | Correct. The same `lua_isstring`-before-`lua_isinteger` ordering also lives in `enum_lua.c` and `bitfield_lua.c`. |
| 4 hooks at target+4 | The analyzer change is sound. The narrative is not: `ExecuteStatsFunctors::AttackTarget` uses plain `DobbyHook`—the only live consumer whose offset moves is the `GetFreeMessage` net hook (+4 → 0). `leading_adrp_skips_forward` codifies the pre-existing unrelocated dirty-window path. |
| 5 Osiris wrappers discard x0 | Correct. A stale "void" comment remains at the `fake_Event` call site. |

Integration path, same as #91 in July: this lands on `integration/pr-101` with your two commits cherry-picked, authorship preserved, followed by lead commits that rework defect 2 as an in-serializer `LUA_TUSERDATA` case with a cycle guard, fix the test attribution for defect 4, extend the enum fix to the other two sites, and make the PersistentVars fixture hermetic (it leaves `HOME` changed and shells out to `rm -rf`). The duplicate `src/osiris` include in `CMakeLists.txt` goes too. Live verification on 4.1.1.7398727 follows, then v0.44.0. This PR closes as integrated once the branch merges to main.
```

### pr-103.md

- VERDICT: POST WITH EDITS
- FACTS: all confirmed.

```md
Merging as-is: 23 tests, no source changes, CI green, no overlap with #101. Thank you @mikowals.

Review notes (Codex gpt-5.6-sol, read-only, full report in `docs/reviews/2026-09-14-codex/pr-103-review.md`), landing as a follow-up commit rather than a revision request:

- Three comments overclaim what the test catches. The `read_string` loop-bound mutation (`i < max_len`) is not an overflow, because `safe_memory.c` rewrites the last byte with NUL. The GPU pre-check stays mutation-equivalent, since the kernel rejects the address anyway. `sub_id_carries_full_salt_and_index` never touches `pool_pack()`, because `entity_events.c` is not linked into tier 0.
- The canaries are adjacent struct members—safe under current code, undefined under an overflowing mutant. Switching to one backing array with a smaller logical size.
- `(int)0xFFFFFFFF` is implementation-defined—`-1` says the same thing.

Two real findings surfaced by your tests get their own issues: subscription-handle ABA after the uint16 salt rolls over (both `entity_events.c` and `ecs_system_update.c`), and NULL contracts for the mod path helpers.
```

### pr-91.md

- VERDICT: POST WITH EDITS
- FACTS: all confirmed. Every cited SHA is on `main`, its description matches the claim, and `3368581`/`452fb9f` preserve @mikowals’s authorship.

```md
#91 is integrated through #95. The per-version offset table and routed call sites landed in `3368581`, preserving your authorship. The patch-equivalent cherry-pick `452fb9f` brought in the porting tool, manifest and guide. Main then hardened the design in `d16a5dc` and extended it to 4.1.1.7398727 in `01b6b91`. The safety work is present too: StaticData hooks are exact-version gated with TypeContext fallback, Template uses table-backed pointer reads instead of live Dobby hooks, and MAP_JIT trampoline writes are protected. Cherry-picking the original commits now reintroduces the 6995620/7209685 structures that the typed table replaced. Thank you @mikowals for the architecture, tooling and crash analysis. Closing.
```

## Summary

The recurring fixes removed one hedged diagnosis, repeated technical nouns, spaced-out clause fragments, prose semicolons, and a pithy PR closer; the fact pass also corrected the v0.39.0 date, an invalid overlay path, two references to a commit absent from `main`, the defect-4 overclaim, and wording that implied the Nexus ID was already fixed. Every revision remains at or below its draft’s word count. No files were modified.
