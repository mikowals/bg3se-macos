I’ll perform a strictly read-only triage. I’m using the bg3se-macos Ghidra skill because it contains repository-specific guidance; I’ll read that and every supplied input in full before cross-checking claims against main.
The repository-specific skill confirms the relevant fault lines: ARM64 hook ABI, ECS layout, Osiris listeners, Lua API gaps, and the intentionally absent client/UI surface. I’m now reading the full community record, then I’ll distinguish observed failures from fork release claims and verify each claim against source.
All requested issue files and the fork inventory are now read in full. The thread’s strongest signal is already clear: the complaints after August 21 overwhelmingly concern mageweaver releases and its mod manager, while marcus-sa explicitly reports his affected mods worked on tdimino main. I’m validating that separation against main’s reports, source, and test tiers now.
Main is materially further along than the old issue bodies imply: MCM has a live v0.41 baseline with 27/27 steps, while Ext.UI and DAP are explicitly excluded from the supported parity denominator. The fork comparison is also exposing genuine main defects—most notably SpellMeta’s current 80-byte stride versus the fork’s claimed 0x60—and several claims that are additions rather than equivalents already on main.
The decisive distinction is now clear: main has a reproducible compatibility record, while #102 mixes fork support, configuration errors, and genuine runtime defects. I’m tightening the remaining citations and separating verified absence from claims that cannot be proven from the available fork metadata.
## Answer

Main should remain the supported release line. The #102 thread contains no concrete defect reproduced on tdimino main; its failures are overwhelmingly reports against mageweaver releases or that fork’s mod manager. Merge PRs #101 and #103, rerun the compatibility matrix, and invite small, tested PRs from both forks. Do not bulk-cherry-pick fork histories.

Main is currently v0.43.0 for BG3 4.1.1.7398727 ([version.h](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/core/version.h:9), [version_detect.h](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/core/version_detect.h:25)). Its documented parity is approximately 94.8%, with stubs scoring zero ([ROADMAP.md](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/ROADMAP.md:5)).

| Issue | Class | Recommended action | Effort |
|---|---|---|---:|
| #102 | Fork/community consolidation | Post boundary-setting reply; keep briefly open for upstream PR coordination, then close | M |
| #99 MCM | **WORKS-ON-MAIN** | Close as supported; document load order and hotkey | S |
| #98 BG3SX | **NEEDS-VETTING** | Keep open; add a dedicated live scenario | M |
| #97 mod nominations | Tracker | Keep open; mark completed targets and retain unvetted nominees | M |
| #70 startup | Partially implemented | Keep open, narrowed to budgets and optimization | M |
| #42 debugger/DAP | Explicitly deferred | Close; require a new scoped proposal to revive | H |
| #35 Noesis UI | Explicitly deferred | Close; preserve research documentation | H |
| #8 technical debt | Superseded umbrella | Fold remaining items into ROADMAP/#70 and close | S |

Effort describes implementation work, not the administrative act of closing an issue.

## Evidence

### Issue #102 chronological digest

1. Mageweaver announces a fork claiming roughly 97% parity ([issue-102.md](</private/tmp/claude-501/-Users-tomdimino-Desktop-Programming/game-modding-bg3/bg3se-macos/60cd31ce-019d-4c7c-9024-600e8edb88ff/scratchpad/pr-review/issues/issue-102.md:5>)).
2. Marcus asks whether MCM works.
3. Apollo reports the fork does not work; mageweaver acknowledges an introduced bug and posts a fix.
4. Mageweaver announces a beta macOS mod manager.
5. Apollo gets an MCM button, but the panel does not open; mageweaver points to the backslash shortcut.
6. Kabo confirms MCM 1.40.1 loads and emits the gear sound, but no panel appears.
7. Kabo corrects load order—MCM and Gustav first—but the button still fails; keyboard shortcuts are identified.
8. Mageweaver reports a working extender socket and 755 detected mods.
9. v0.44.1 and a video installation tutorial are announced.
10. Apollo reports MCM opens once, then stops responding.
11. The mod manager incorrectly disables base modules and injects GustavX; fixes are released.
12. Apollo resolves the MCM shortcut by changing `INSERT` to `F10` on an Italian keyboard.
13. Apollo reports BG3SX finds no NPCs; mageweaver says BG3SX should work on v0.44.2.
14. Reinstallation does not fix BG3SX’s empty NPC list.
15. Marcus says almost none of his mods work on the fork although they worked on tdimino main.
16. Apollo downgrades to v0.44.1.
17. v0.46.0 is announced.
18. Apollo reports invisible characters, NPCs, environment, and MCM under v0.46.
19. The visual failure is later fixed, but BG3SX remains broken.
20. Marcus reports failures in Sit This One Out 2, Tutorial Chest, 5e Spells, Transmog Enhanced, Twin Blades, body mods, Arch Traitor’s Armor, and modded gear.
21. Apollo reports BG3SX scenes do not start.
22. Marcus announces his `fix/ext-vars-table-identity` branch and later says it contains many fixes.
23. Users report the mod manager hanging and requesting a PAK when given a Script Extender library.
24. Mageweaver later claims Transmog, MCM, AEE, and custom companions work.
25. Apollo reports EasyCheat renders black and switches to CrossOver.

### Concrete complaints

No concrete negative report in #102 is attributable to tdimino main. The only direct comparison says the affected mods worked on tdimino main, and Sit This One Out 2 worked with main PR #93 ([issue-102.md](</private/tmp/claude-501/-Users-tomdimino-Desktop-Programming/game-modding/bg3/bg3se-macos/60cd31ce-019d-4c7c-9024-600e8edb88ff/scratchpad/pr-review/issues/issue-102.md:228>), [issue-102.md](</private/tmp/claude-501/-Users-tomdimino-Desktop-Programming/game-modding/bg3/bg3se-macos/60cd31ce-019d-4c7c-9024-600e8edb88ff/scratchpad/pr-review/issues/issue-102.md:266>)). Main’s v0.39 release record independently says MCM was vetted as working after PRs #91/#93/#95 ([ROADMAP.md](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/ROADMAP.md:1540)).

#### Evidence about mageweaver or its manager

| Mod/product | Symptom | Observed on | Reporter |
|---|---|---|---|
| Fork loader | Initially “does not work” | Mageweaver, pre-v0.44.1 | Apollo |
| MCM | Button appears but panel does not open | Mageweaver, version unstated | Apollo |
| MCM 1.40.1 | Gear sound/dialog occurs, no panel | Mageweaver, version unstated | Kabo |
| MCM | Opens once, then button/hotkey stop working | Mageweaver v0.44.1-era | Apollo/VladiX7 |
| MCM | `INSERT` unusable on Italian keyboard; F10 works | Mageweaver v0.44.1-era | Apollo |
| BG3 Mod Manager | “Disable all” also disables Gustav/base modules | Manager before 1.01 | Mageweaver |
| BG3 Mod Manager | Incorrect GustavX injection | Manager before 1.02 | Mageweaver |
| BG3 Mod Manager | Load order resets | Manager 1.04 | Apollo |
| BG3SX | NPC scan returns an empty list | Mageweaver v0.44.2 | Apollo |
| BG3SX | Scenes do not start | Mageweaver v0.46-era | Apollo |
| Rendering generally | Player, NPCs, environment, and MCM become invisible | Mageweaver v0.46.0 | Apollo |
| Sit This One Out 2 | Selected companions do not disappear | Mageweaver v0.46-era | Marcus |
| Tutorial Chest + 5e Spells + Transmog | Changes appear only on newly created characters | Mageweaver v0.46-era | Marcus |
| Twin Blades | Weapons unavailable except pre-equipped variants | Mageweaver v0.46-era | Marcus |
| Body mods | Incorrect body rendering/selection | Mageweaver v0.46-era | Marcus |
| Arch Traitor’s Armor/weapons | Equipping items bricks the UI | Mageweaver v0.46-era | Marcus |
| Modded gear generally | Most gear is missing | Mageweaver v0.46-era | Marcus |
| BG3 Mod Manager | Application becomes non-responsive | Version unstated | xxvoltron |
| BG3 Mod Manager | Requests a PAK when given an SE library | Version unstated | xxvoltron |
| EasyCheat | Screen renders black | Mageweaver v0.47.2-era | Apollo |

These reports occupy the fork-support portion of [issue-102.md](</private/tmp/claude-501/-Users-tomdimino-Desktop-Programming/game-modding-bg3/bg3se-macos/60cd31ce-019d-4c7c-9024-600e8edb88ff/scratchpad/pr-review/issues/issue-102.md:49>) through [issue-102.md](</private/tmp/claude-501/-Users-tomdimino-Desktop-Programming/game-modding-bg3/bg3se-macos/60cd31ce-019d-4c7c-9024-600e8edb88ff/scratchpad/pr-review/issues/issue-102.md:381>).

### Mageweaver claims versus main

`HAS` means an equivalent main implementation is visible. `LACKS` means the searched main surface does not implement the claimed behavior. `UNKNOWN` means main contains a plausible path, but the release-note claim cannot be matched without the fork diff or a live result.

| Fork claim | Main | Evidence |
|---|---|---|
| VT null-tileset crash guard | **LACKS** | Main only exposes virtual-texture resource names; VT support is explicitly excluded ([resource_manager.c](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/resource/resource_manager.c:36), [deferrals.md](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/docs/deferrals.md:91)). |
| Array-buffer session-load guard | **LACKS** | Main validates null/oversized arrays but has no capacity/session guard equivalent ([component_property.c](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/entity/component_property.c:957), [component_property.c](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/entity/component_property.c:1270)). |
| Localization updates | **HAS** | Main implements update and verification paths ([localization.c](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/localization/localization.c:353), [ROADMAP.md](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/ROADMAP.md:61)). |
| First-boot localization replay cache | **LACKS** | Main’s localization path updates the live repository but contains no replay cache ([localization.c](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/localization/localization.c:353)). |
| Stderr capture and shader diagnostics | **LACKS** | Main emits diagnostics to stderr but has no capture/shader-diagnostic subsystem ([main.c](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/injector/main.c:4622)). |
| ServerItem status transfer | **LACKS** | Main aliases `ServerItem` to the server item entity but implements no status-transfer operation ([entity_system.c](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/entity/entity_system.c:2563)). |
| Osiris alias-type crash fix | **LACKS** | Return conversion collapses custom types to strings while dispatch retains declared subtype IDs; no alias normalization exists ([main.c](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/injector/main.c:1937), [main.c](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/injector/main.c:2091)). |
| Mod timers on server frame | **LACKS** | Timer work is driven from the hooked Osiris event path, not a server-frame system update ([main.c](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/injector/main.c:3822)). |
| Native story function calls | **HAS** | Main dispatches calls through `DivFunctions::Call` ([main.c](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/injector/main.c:2261)). |
| “Story procs actually run” | **UNKNOWN** | Main contains a PROC dispatch path, but the supplied evidence has no matching live verification ([main.c](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/injector/main.c:2273)). |
| Database and PROC listeners fire | **LACKS** | Main can call/query DBs and procs, but its listener hook is the `COsiris::Event` path rather than DB/PROC node callbacks ([main.c](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/injector/main.c:2285), [lua_osiris.c](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/lua/lua_osiris.c:49)). |
| Correct PersistentVars | **LACKS** | Main’s serializer currently reads the buffer placeholder as the value, producing `null`; PR #101 fixes it ([lua_persistentvars.c](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/lua/lua_persistentvars.c:342)). |
| Persistent variables scoped per playthrough | **LACKS** | Main stores per-mod JSON beneath one global data path, not a savegame identity ([lua_persistentvars.c](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/lua/lua_persistentvars.c:2)). |
| Osiris tuple rejection fix | **UNKNOWN** | Main has a hard-coded tuple representation; the exact fork failure is not available for comparison ([main.c](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/injector/main.c:1528)). |
| Live component-size oracle/compiler-derived layouts; 357 corrected fields | **LACKS** | Main’s generator estimates layouts and warns that generated offsets require verification ([generate_component_stubs.py](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/tools/generate_component_stubs.py:271), [ROADMAP.md](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/ROADMAP.md:194)). |
| 181 hidden tag components and all 103 boost components | **LACKS** | Main records only 114 tag components and has incomplete generated-layout verification ([component_offsets.h](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/entity/component_offsets.h:1002), [ROADMAP.md](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/ROADMAP.md:194)). |
| SpellMeta stride `0x60`, not `80` | **LACKS** | Main explicitly defines an 80-byte SpellMeta stride ([component_property.h](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/entity/component_property.h:51), [component_offsets.h](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/entity/component_offsets.h:713)). |
| DifficultyCheck/IsInsideOf and additional typed arrays | **LACKS** | Main’s relevant entries remain estimated or dynamically opaque ([component_offsets.h](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/entity/component_offsets.h:2900), [component_offsets.h](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/entity/component_offsets.h:7757)). |
| Nested DisplayName shape | **LACKS** | Main exposes the list as a dynamic/unknown layout ([component_offsets.h](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/entity/component_offsets.h:4754)). |
| Root-template serialization and shared-bank write refusal | **LACKS** | Main’s Lua template surface provides lookup/pointers; global-index handling remains unimplemented, and generic serialization targets component proxies ([lua_template.c](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/lua/lua_template.c:27), [template_manager.c](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/template/template_manager.c:642), [lua_ext.c](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/lua/lua_ext.c:1007)). |
| Writable appearance arrays, array assignment, CustomName/STDString, CCTO | **LACKS** | Main’s component writer supports a limited primitive/array set and refuses unknown layouts ([component_property.c](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/entity/component_property.c:1270), [ROADMAP.md](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/ROADMAP.md:1539)). |

The source of the fork claims is [forks.md](</private/tmp/claude-501/-Users-tomdimino-Desktop-Programming/game-modding/bg3/bg3se-macos/60cd31ce-019d-4c7c-9024-600e8edb88ff/scratchpad/pr-review/forks.md:69>).

### Marcus branch and PR #101 overlap

The input says Marcus is 317 commits ahead but enumerates only 60 commit subjects ([forks.md](</private/tmp/claude-501/-Users-tomdimino-Desktop-Programming/game-modding/bg3/bg3se-macos/60cd31ce-019d-4c7c-9024-600e8edb88ff/scratchpad/pr-review/forks.md:2>), [forks.md](</private/tmp/claude-501/-Users-tomdimino-Desktop-Programming/game-modding/bg3/bg3se-macos/60cd31ce-019d-4c7c-9024-600e8edb88ff/scratchpad/pr-review/forks.md:65>)). Therefore only those 60 can be counted:

| Theme | Visible commits | Disposition |
|---|---:|---|
| Render/shader guards | 16 | High-risk; squash diagnostic/alias oscillation and submit by crash signature |
| Entity/component/lifetime | 19 | Upstreamable when split by behavior with layout/live tests |
| Vars/serialization | 5 | Best first functional series, split identity from save lifecycle |
| Osiris | 7 | Upstreamable after removing logging/revert churn and adding listener/ABI tests |
| StaticData/template/resource/stats/enum | 11 | Strong source of atomic PRs |
| Docs/harness | 1 | Directly upstreamable |
| Cross-cutting diagnostic cleanup | 1 | Churn, not a feature PR |
| Not enumerated in the supplied input | 257 | Unclassifiable |

At least 11 of the 60 visible commits are explicitly `diag`, `log`, `revert`, or diagnostic cleanup. The render alias sequence also changes direction repeatedly and must be squashed.

PR #101’s five defects do not have exact subject-level duplicates in the visible Marcus list:

| PR #101 fix | Marcus overlap |
|---|---|
| PersistentVars serializes as `null` | Same domain as `dbce0cfa`, `ab989daf`, `6d75edd3`, and `ed0b9f91`, but none names the buffer-index defect |
| JSON component proxies stringify as `null` | Adjacent to `ef558e04` nested-table stack reservation, not the same stated defect |
| `Ext.Enums.X[n]` always returns nil | Separate from `1124c1e6` Windows `SpellFlags` spellings |
| Hooks install at target+4 | No visible Marcus equivalent |
| Osiris wrappers discard x0 | No visible Marcus equivalent |

Main’s five defects are directly visible in [lua_persistentvars.c](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/lua/lua_persistentvars.c:342), [lua_json.c](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/lua/lua_json.c:265), [enum_ext.c](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/enum/enum_ext.c:18), [arm64_decode.c](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/hooks/arm64_decode.c:350), and [main.c](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/injector/main.c:3270).

Recommended extraction order:

1. Merge #101 and #103 first; require Marcus to rebase.
2. Ask for `4970683e` bank-index correction as one probe-backed PR.
3. Ask for `1124c1e6` enum spellings and `fb2d89d5` component registrations as separate PRs.
4. Split variables into:
   - `ed0b9f91` + `ef558e04`: identity/stack safety.
   - `dbce0cfa` + `ab989daf` + `6d75edd3`: save-scoped persistence lifecycle.
5. Accept Osiris work by behavior, not as one cluster.
6. Do not cherry-pick any SHA directly from subjects alone. Review actual diffs and tests through focused PRs.
7. Do not bulk-import render/shader commits; their diagnostic/revert history signals an experimental sequence.

### Maintainer stance

- Main is the canonical supported line.
- Mageweaver is an experimental compatibility fork with valuable fixes but a demonstrated release/support churn.
- Marcus’s branch is a high-value research branch, not a merge unit.
- General users should be pointed to main. Fork users should report failures to the corresponding fork with exact extender, manager, mod, and game versions.
- Both fork maintainers should be invited to upstream focused fixes with a reproducer, regression test, and ARM64 ABI/layout evidence.

## Details

### #99 — Mod Configuration Menu

**Classification: WORKS-ON-MAIN. Close.**

Main’s v0.41 MCM baseline completes 27/27 assertions, including viewport state, PersistentVars state, ModEvents roundtrip, listeners, Tick, and `DB_Players` ([mcm.json](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/tools/bg3se_harness/scenarios/mcm.json:1), [baseline/mcm.json](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/docs/compat-reports/baseline/mcm.json:1), [baseline/mcm.json](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/docs/compat-reports/baseline/mcm.json:1476)). The compatibility catalog and supported-mod list both mark it working ([popular_mods.json](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/tools/bg3se_harness/catalog/popular_mods.json:5), [supported-mods.md](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/docs/supported-mods.md:39)).

The issue thread ultimately identifies load order and a non-US `INSERT` binding, then reports success using F10—but on mageweaver, not main ([issue-99.md](</private/tmp/claude-501/-Users-tomdimino-Desktop-Programming/game-modding-bg3/bg3se-macos/60cd31ce-019d-4c7c-9024-600e8edb88ff/scratchpad/pr-review/issues/issue-99.md:14>)).

The likely blocker for the original symptom was configuration/load order. Main’s current outstanding functional risk is PR #101’s PersistentVars serialization defect, so MCM should be rerun after that merge. It is not evidence that MCM is presently blocked.

Documentation defect: `supported-mods.md` associates MCM with Nexus ID 8901, while the catalog and vetting plan use 9162 ([supported-mods.md](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/docs/supported-mods.md:41), [popular_mods.json](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/tools/bg3se_harness/catalog/popular_mods.json:5), [vetting plan](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/docs/plans/2026-05-02-001-feat-systematic-top5-mod-vetting-plan.md:34)).

### #98 — BG3SX

**Classification: NEEDS-VETTING. Keep open.**

There is no BG3SX scenario, baseline, or compatibility report. The thread establishes only that scenes do not start and includes an unverified MCM-dependency theory ([issue-98.md](</private/tmp/claude-501/-Users-tomdimino-Desktop-Programming/game-modding-bg3/bg3se-macos/60cd31ce-019d-4c7c-9024-600e8edb88ff/scratchpad/pr-review/issues/issue-98.md:5>)).

The strongest blocker candidate on main is story integration: main can call native PROC/DB functions but lacks database/PROC listener hooks ([main.c](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/injector/main.c:2273), [lua_osiris.c](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/lua/lua_osiris.c:49)). Mageweaver explicitly advertises story-proc and DB/PROC-listener fixes in v0.47.3–0.47.4. Because BG3SX’s scripts and a live trace are absent, the issue cannot yet be classified `BLOCKED-BY`.

Required vet: exact BG3SX/BG3AF versions, clean load order, log capture, API trace, and an assertion that an eligible NPC is discovered and a scene begins. The existing plan already defines the vet/run/matrix pipeline and live assertion standard ([vetting plan](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/docs/plans/2026-05-02-001-feat-systematic-top5-mod-vetting-plan.md:125), [vetting plan](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/docs/plans/2026-05-02-001-feat-systematic-top5-mod-vetting-plan.md:151)).

### #97 — mod nominations

**Recommendation: keep open as the compatibility tracker, but update its body.**

All ten original targets now have main baselines:

| Original target | Main report |
|---|---|
| 5e Spells | [5e_spells.json](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/docs/compat-reports/baseline/5e_spells.json:1) |
| Community Library | [community_library.json](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/docs/compat-reports/baseline/community_library.json:1) |
| Expansion — Level 20 | [expansion_lvl20.json](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/docs/compat-reports/baseline/expansion_lvl20.json:1) |
| MCM | [mcm.json](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/docs/compat-reports/baseline/mcm.json:1) |
| Party Limit Begone | [party_limit_begone.json](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/docs/compat-reports/baseline/party_limit_begone.json:1) |
| Combat Extender | [combat_extender.json](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/docs/compat-reports/baseline/combat_extender.json:1) |
| More Reactive Companions | [more_reactive_companions.json](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/docs/compat-reports/baseline/more_reactive_companions.json:1) |
| Camp Event Notifications | [camp_event_notifications.json](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/docs/compat-reports/baseline/camp_event_notifications.json:1) |
| Auto Send Food to Camp | [auto_send_food.json](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/docs/compat-reports/baseline/auto_send_food.json:1) |
| Always Show Approvals | [always_show_approvals.json](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/docs/compat-reports/baseline/always_show_approvals.json:1) |

Community nominations:

| Nominee | Main report |
|---|---|
| Transmog Enhanced Revamped | **Yes** — [transmog_enhanced.json](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/docs/compat-reports/baseline/transmog_enhanced.json:1) |
| MCM | **Yes** |
| BG3 Sex Framework/BG3SX | **No** |
| BG3AF animation dependency | **No** |
| Demon Hunter Class | **No** |
| Twin Blades | **No** |

The nominations are recorded in [issue-97.md](</private/tmp/claude-501/-Users-tomdimino-Desktop-Programming/game-modding-bg3/bg3se-macos/60cd31ce-019d-4c7c-9024-600e8edb88ff/scratchpad/pr-review/issues/issue-97.md:53>). Four distinct nominated targets remain unvetted, so closing the tracker would discard useful scope.

### #70 — startup optimization

**Recommendation: keep open, narrowed.**

This is partially done. Main logs initialization-phase timings and defers expensive entity/stats/static-data initialization until the runtime is ready ([main.c](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/injector/main.c:3136), [main.c](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/injector/main.c:4669)). The test documentation also quantifies compilation/startup overhead ([testing.md](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/docs/testing.md:459)).

What remains is the epic’s actual outcome: cold/warm launch baselines, a startup budget, ranked phase costs, and verified optimizations. Keep it as the performance owner rather than folding it into a generic roadmap item.

### #42 — debugger/VS Code integration

**Recommendation: close as out of scope.**

Main has logging callback scaffolding, not a debugger or DAP implementation ([logging.c](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/core/logging.c:441), [logging.h](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/core/logging.h:228)). DAP is explicitly excluded from parity accounting and deferred ([ROADMAP.md](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/ROADMAP.md:1435), [deferrals.md](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/docs/deferrals.md:91)). Reopen only through a new proposal with protocol, transport, pause semantics, and thread-safety design.

### #35 — Ext.UI/Noesis

**Recommendation: close as out of scope.**

Main’s `Ext.UI` implementation is a compatibility stub ([lua_ui.c](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/lua/lua_ui.c:1)). Noesis is explicitly excluded from the parity denominator ([ROADMAP.md](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/ROADMAP.md:1151), [deferrals.md](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/docs/deferrals.md:91)). There is useful reverse-engineering documentation for the Noesis/input pipeline, but no implementation ([NOESIS_UI_FRAMEWORK.md](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/ghidra/offsets/NOESIS_UI_FRAMEWORK.md:1)).

### #8 — stability, testing, documentation

**Recommendation: fold residual work into ROADMAP and close.**

The umbrella has largely completed its purpose. The roadmap records pattern-scan, stability, testing, and documentation progress, leaving memory-leak detection and performance benchmarks as identifiable residuals ([ROADMAP.md](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/ROADMAP.md:1444)). The test system now documents 544 checks across Tier 0, harness tests, Tier 1, and Tier 2 ([testing.md](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/docs/testing.md:50)). Keep startup benchmarking under #70 and create a focused leak issue when ownership exists.

## Related

### Draft comment for #102 — 155 words

Thanks to mageweaver, DigitalMinotaur, marcus-sa, and everyone testing these builds.

This repository remains the supported macOS BG3SE line. The failures reported in this thread after the fork announcement are reports against mageweaver releases or its mod manager; they are not evidence of regressions on main. Fork-specific installation and compatibility reports must include the exact fork, extender version, manager version, mod version, and game version and should be filed on that fork.

Main’s next steps are:

1. Land and verify #101’s five correctness fixes.
2. Harden Tier 0 through #103.
3. Rerun the complete compatibility matrix on v0.43.
4. Accept focused, tested fixes from both forks.

We will not bulk-merge or blindly cherry-pick a 317-commit branch. Marcus: please submit the resource-bank index fix first, followed by separate Vars identity and save-persistence PRs. Mageweaver: component-layout/SpellMeta corrections and DB/PROC listener support are strong upstream candidates when accompanied by reproducers and regression tests.

Users seeking the supported build should use releases from this repository. Treat fork releases as experimental.

### Draft comment for #99

MCM is verified on main: the v0.41 compatibility baseline passes all 27 assertions, including viewport state, events, listeners, PersistentVars state, Tick, and `DB_Players`.

Place MCM first in the mod load order. On keyboards where `INSERT` is unavailable or intercepted, change the toggle binding to F10 in MCM’s settings JSON. Mod visibility in a particular manager is separate from Script Extender compatibility.

We will rerun this scenario after #101 lands because that PR corrects PersistentVars serialization. Closing this issue as supported on main. Please open a new issue with the generated compatibility JSON and `bg3se.log` if the main release fails.

### Draft comment for #98

BG3SX has not been vetted on main, so this remains open as a compatibility target.

MCM itself is verified and is not an established blocker. The leading main-side gap is story integration: main can call native Osiris functions, but database/PROC listener behavior has not been implemented or live-verified. That matches the failure mode where NPC discovery or scene startup never advances.

Please provide the exact BG3SX and BG3AF versions and a clean load order. The acceptance test will require both an eligible-NPC discovery assertion and successful scene startup, with the compatibility report and extender log attached.

Classification: **NEEDS-VETTING**.

### Draft comment for #97

Tracker update:

- All ten original targets now have compatibility baselines.
- MCM and Transmog Enhanced Revamped are verified on main.
- BG3SX, BG3AF, Demon Hunter Class, and Twin Blades remain unvetted community nominations.

Keeping this issue open as the mod-vetting tracker. The body should be updated to link completed reports and move the four remaining nominations into the active queue. New nominations should include the exact Nexus version, dependencies, expected in-game behavior, and a save/setup capable of exercising that behavior.

### Draft comment for #70

Startup instrumentation and deferred initialization are now implemented, but the optimization goal is not complete.

Keeping this issue open with a narrower scope: establish cold/warm startup baselines, define a startup budget, rank initialization phases by cost, and land only changes with before/after measurements. Harness-test compilation and the deferred entity/stats/static-data phases are the first measurement targets.

### Draft comment for #42

Closing this epic as out of current scope. Main has logging callbacks but no debugger transport, DAP server, breakpoints, stepping, pause model, or variable inspection, and DAP is explicitly listed as a parity exclusion.

A future implementation should begin as a new design issue covering protocol transport, Lua hook behavior, game-thread suspension, and server/client context safety.

### Draft comment for #35

Closing this epic as out of current scope. `Ext.UI` remains an intentional compatibility stub, and Noesis integration is excluded from current parity accounting. Existing Noesis/input-pipeline reverse-engineering documentation should remain as the starting point for any future implementation.

MCM compatibility is handled through the supported ImGui/runtime surface and does not require completing this epic.

### Draft comment for #8

Closing this umbrella issue and moving its remaining work to focused owners.

Pattern scanning, tiered testing, API/architecture documentation, and the major stability work are now represented in the repository and ROADMAP. The remaining actionable items are performance benchmarking, already owned by #70, and memory-leak detection, which should receive a dedicated issue when scheduled.

This issue is now superseded by the ROADMAP and focused subsystem issues.
