# Live Verification Session — 2026-09-14/15

Phase 5 of `docs/plans/2026-09-14-001-chore-pr-issue-triage-v0430-plan.md`: the first
live session on game build **4.1.1.7398727** (the 2026-08-04 migration landed offline only),
with the #101/#103 integration and the Codex review follow-ups loaded. Seven launches (two stalled at the menu, see Observations); the
evidence below names the PID each result came from.

## Session Identity

| Field | Value |
|-------|-------|
| Game build | 4.1.1.7398727 (arm64 LC_UUID `0C51CAED-6D60-3DCD-9299-8519C92631B0`, footer `v4.1.1.7398727` on the main menu) |
| BG3SE dylib | v0.43.0 source, `main` `beb1e38` + this pass; final deploy 2026-09-15 00:19 (5.6 MB) |
| PIDs | 17150 (pre-fix dylib, diagnosis), 60203 (first fix batch), 66819 and 94363 (menu stalled, no results used), 88202 (MCM attempt 1, stalled), 91150 (MCM 27/27), 98372 (final tiers on the 00:19 build) |
| Handshake | `!identity` → `session_init:"complete"`, `stats_ready:true`, `game_state:"Running"` on every PID that produced a result |
| Save | most recent via `-continueGame`, host `b4d01c83-25e9-6156-d91b-84e619b3757d` |
| Injection | harness `launch --continue` (build → deploy → `insert_dylib` patch → launch) |
| Crash reports | none for any PID this session (`~/Library/Logs/DiagnosticReports`, `crash.log` empty) |

## Results

- **Tier 1 (`!test`):** 114/114 on PID 17150, 114/114 on the 00:07 build, 114/114 on the 00:19 build (PID 98372)
- **Tier 2 (`!test_ingame`):** 106/110 on 17150 (pre-fix), 106/110 on the 00:07 build, **108/110 on the 00:19 build (PID 98372)** — the two remaining failures are the environmental damage-functor probe and the gated ValueList insert (Triage 1–2)
- **MCM (`compat run mcm --launch`):** 27/27 steps passed on PID 91150 (run `mcm_1789446019`, `compat diff mcm` against the 2026-07-30 baseline: no regressions, no added or removed steps; report archived as `docs/compat-reports/mod_configuration_menu_1789446019.json`). The first launch attempt of the scenario stalled at the main menu (see below); the built-in retry loaded.

### Plan-mandated probes (defects from #101, live)

| Probe | Result | PID |
|-------|--------|-----|
| PersistentVars round trip (defect 1) | marker `live_1789443627`, `num` 42, nested list read back after `SyncPersistentVars()` + restore; on-disk `persistentvars/BG3SE_LIVE.json` is `{"num":42,"nested":{"list":[1,2,3]},"marker":"live_1789443627"}` (63 bytes, not the four-byte `null`) | 17150 |
| `Ext.Enums.DamageType[7].Label` (defect 3) | `Fire`; `DamageType[0]` → `None`; `DamageType['7']` → nil; `AttributeFlags[0x2] \| 0x4` → `__Value` 6; `AttributeFlags[0x800000]` → nil (mask reject) | 17150, 60203, final |
| `Ext.Json.Stringify(entity.Health)` (defect 2) | `{"Hp":26,"MaxHp":32,"TemporaryHp":0,"MaxTemporaryHp":0,"IsInvulnerable":false}`; plain-table golden `[1,"two",true,{"k":2.5}]`; `t.self = t` → `{"self":null}`; branching cycle `{"b":null,"a":null}` | 17150, final |
| Ext.Net after the analyzer change (defect 4) | `Ext.Net.IsReady()` true, `IsHost()` true. No live `GetFreeMessage` hook is installed on this build (net hooks are deferred, log: "game net untouched"), so the +4 → 0 move has no live consumer to exercise; the analyzer's only other candidate, `FeatManager::GetFeats`, is Dobby-hooked at +0x0 unchanged | 17150, final |
| Osiris return forwarding (defect 5) | Save reached `Running`; `Ext.Debug.GetHookStatus()` → `hook_count` 4, `functor_hooks_installed` 10, `version_gated` false, `hooks_fired` 6443 (17150) / 2767 (final) | 17150, final |
| `entity:GetReplicationFlags('Health')` | nil (fail-closed, gate closed on 7398727) — observation only, no credit | 60203 |

### Component-proxy diagnosis (plan tasks #65/#67) — three real defects found and fixed

| Symptom (PID 17150) | Root cause | Fix | Verified (final dylib) |
|--------|-----------|-----|-----------|
| `entity.Uuid` nil | Generated TypeId discovery never wrote the discovered index into hand-written layouts: `ls::uuid::Component` discovered as TypeIndex 2082, layout stayed 0 | generator + `generated_component_registry.c` call `component_property_set_type_index()` | layout TypeIndex 2082, `e.Uuid` → `Component<Uuid>` |
| `Uuid.EntityUuid` = `831cd0b4-e925-5661-…` (host is `b4d01c83-25e9-6156-…`) | Four GUID formatters printed bytes in memory order | all property GUIDs route through `guid_to_string()` | `EntityUuid == HandleToUuid == Osi host GUID` → `true` |
| `Transform.Translate` nil, `Position` held the quaternion | Legacy struct had position first; raw dump at the component address: `+0x04 -0.723 +0x0c 0.691` (quat), `+0x10 -44.824 +0x14 25.499 +0x18 -138.637` (translate), `+0x1c..0x24 1.0` (scale) | struct reordered `{rot[4], pos[3], scale[3]}`; `Translate`/`RotationQuat` added | `Translate = -44.82 25.50 -138.64`, `RotationQuat = 0 -0.723 0 0.691` |
| `GetEntitiesAroundPosition(host, 5)` missed the host | Consequence of the Transform order | — | 288 entities within 5 m, host present |
| `GetAllEntitiesWithUuid()` → `{}` | Guard required `Keys.size == Values.size`; live map at `0x146f7f840`: `Keys.size` 23151, `Keys.capacity` 32768, `Values.size` 32768 (`UninitializedStaticArray` sized to capacity) | guard relaxed to `Keys.size <= Values.size` | `Wave7.Entity.GetAllEntitiesWithUuid` PASS on PID 98372 (host present, >100 entries) |

### SpellMeta stride probe (fork claim from #102)

`SpellContainer.Spells` on the host, PID 17150: consecutive `SpellId` FixedStrings at
+0x00, +0x60, +0xC0, +0x120 — stride **0x60 (96)**, not 80. Fixed in
`component_offsets.h`; on 60203/final the first five elements resolve `Projectile_Jump`,
`Target_Dip`, `Shout_Hide`, `Target_Shove`, `Throw_Throw` with `__size` 96 and `SpellId`
exposed on each element (42 spells on the host).

## Failure Triage

Tier 2 on the pre-fix and 00:07 dylibs (4 failures each):

1. **`Stats.DamageEvents.PairedFiring`** — environmental: the probe needs a damage functor
   observed in-session ("apply BURNING to the host"). Not a defect; same on 2026-08-04.
2. **`Wave7.Stats.AddEnumerationValue`** — expected: `ValueList::Insert` is gated closed on
   7398727 (`VALUELIST_INSERT_VERIFIED_BUILD`), log line "ValueList::Insert disabled for
   unaudited game version". Stays closed; no gate change this pass.
3. **`Wave7.Entity.GetAllEntitiesWithUuid`** — REAL, the size-guard defect above. Fixed
   after the 00:07 run.
4. **`Wave7.Entity.GetEntitiesAroundPosition`** — on 17150 REAL (Transform order); on the
   00:07 build the host-within-5 m assertion passed and the failure moved to line 51: the
   test's zero-radius call indexed the `{x,y,z}` table as `pos[1..3]`. Test fixed.

## Observations that are not defects

- **PAK load errors** for BG3MCM, CommunityLibrary, Expansion, REL_SE and AutoSendFoodToCamp
  (`attempt to index a nil value`, `Unknown component type: CombatantJoinEvent`,
  `Lifetime of StatsObject has expired`) appear identically in the pre-fix session (18 lines)
  and the final one — pre-existing on main, not introduced by this pass. They are the
  next triage list for #99/#98.
- **Menu stall (PIDs 66819 and 88202):** twice, `-continueGame` did not auto-load and the
  main menu ignored the harness watchdog's six in-process clicks, a CGEvent click, a
  CGEvent Return and a direct `!click` (all logged as posted to `LSMTLView mouseDown:`).
  Both stalls followed a **graceful quit** of the previous instance (harness `quit`); every
  launch that followed a SIGTERM auto-loaded (launches 3, 5, the MCM retry). `modsettings.lsx`
  was not rewritten (mtime 2026-08-04), so the load-order hypothesis is out. The compat
  runner's built-in retry (kill + relaunch) is the workaround; root cause open, noted for
  the harness backlog.

## Process notes for the next session

- **Never run `pytest tests/harness/` while a live session is up.** Two launches died at
  exactly the moment a pytest run started (23:55:19 and 00:00:47; exit −15 = SIGTERM, no
  crash report). The lifecycle tests terminate what they take to be a harness-owned PID.
  The third launch, with nothing else running, loaded cleanly.
- A game that vanishes without a crash report may have been quit from the Dock: PID 17150
  ended on a "Quit AppleEvent" in the unified log (`/usr/bin/log show`), not a crash.
- Harness `eval`/`run` do not echo `return` values in multi-line mode; wrap probes in
  `pcall` and `_P('TAG …')`, then read `logs/latest.log`.
- Overwriting the deployed dylib while the game has it mapped is avoidable: quit first,
  then `cmake --build build`.
