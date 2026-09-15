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
- **Menu stall (PIDs 66819, 88202; Phase 6: 39113, 48071):** `-continueGame` did not
  auto-load and the main menu ignored the harness watchdog's six in-process clicks, a
  CGEvent click, a CGEvent Return and a direct `!click` (all logged as posted to
  `LSMTLView mouseDown:`). The Phase 5 stalls followed a **graceful quit**; in Phase 6,
  48071 stalled after a SIGTERM too, so the trigger is not simply the previous exit
  path. The menu shows MCM's "Your load order is likely being reset" banner during a
  stall. `modsettings.lsx` was not rewritten (mtime 2026-08-04). SIGTERM + relaunch
  has loaded the save on the next attempt every time (Phase 6: 42508, 51375); the
  compat runner's built-in retry does the same. Root cause open, harness backlog.

## Phase 6 (2026-09-15 morning) — StaticData banks, #100 type, #88 probe

Same game build; dylib rebuilt at 07:34 (type registration) and 07:44 (bank
resolution). PIDs: 39113 and 48071 stalled at the main menu (see the pattern
below), 42508 ran the diagnosis, 51375 ran the verification.

### The TypeContext "manager" is a TypeId global, not a bank

`Ext.StaticData.DumpStatus()` on 42508 (pre-fix path) showed what the metadata
accessors were reading: `Feat count=0`, `Race count=0 ptr_array=0x116`,
`Class count=24576 ptr_array=0x75`, `Origin count=1 ptr_array=0x10300000101`.
The slot pointers line up with `nm` on the frozen binary:

| TypeContext slot (runtime) | Unslid | Symbol |
|---|---|---|
| `eoc::FeatManager` `0x10cd9fc30` | `0x108927c30` | `ls::TypeId<eoc::FeatManager, ls::ImmutableDataHeadmaster>::m_TypeIndex` |
| `eoc::CharacterCreationAppearanceVisualManager` `0x10cd9fac0` | `0x108927ac0` | `ls::TypeId<…AppearanceVisualManager, ls::ImmutableDataHeadmaster>::m_TypeIndex` |

So every "HASHMAP" read since the Dec 2025 rewrite dereferenced neighbouring
globals; on this build `GetAll()` returned `{}` for Feat/Race/God/Progression
and garbage past the first entry elsewhere. Only `ActionResource`, captured by
its Get<T> hook, held real entries, and its 0x80 stride was also wrong.
`Ext.StaticData.HashLookup()` on 42508 resolved Feat/Race/God/FeatDescription
through the headmaster table with correct first entries
(`AbilityScoreIncrease d215b9ad-9753-4d74-8ff9-24bf1dce53d6`, `Selune`, `Shar`),
which fixed the design: resolve every bank by type index, never read the slot.

### Live strides (vtable-repeat walk of `Values.buf`)

| Type | type_index | Count | Stride | Was configured |
|------|-----------|-------|--------|----------------|
| Feat | 37 | 41 | 0x128 | 0x128 |
| Race | 54 | 156 | 0x168 | 0x200 |
| Background | 7 | 22 | 0x70 | 0x80 |
| Origin | 48 | 27 | 0x190 | 0x180 |
| God | 38 | 24 | 0x60 | 0x60 |
| Class | 23 | 70 | 0x110 | 0x100 |
| Progression | 52 | 1004 | 0x148 | 0x200 |
| ActionResource | (hook) | 87 | 0x60 | 0x80 |
| FeatDescription | 36 | 41 | 0x60 | 0x80 |
| CharacterCreationAppearanceVisual | 11 | 1315 | 0xA8 | 0xA8 (Windows-derived) |

Post-init on 51375: `TypeContext resolved 10 slots`, `Hash lookup resolved 9
banks` (ActionResource already held by its hook), `10/10 managers ready`; the
runtime logged and overrode the two stale constants still in the table
(Origin, Progression).

### Value-level results (PID 51375, 07:44 build)

- Every type: `GetCount == #GetAll`, zero nil GUIDs, v4 GUID nibble on 100% of
  entries except Progression 1003/1004 and CharacterCreationAppearanceVisual
  1314/1315. Named types resolve names on every entry (Race 156/156: `Humanoid`,
  `Human`, `Elf`…; Class 70/70: `Barbarian`, `BerserkerPath`…; ActionResource
  87/87: `ActionPoint`, `BonusActionPoint`, `ReactionActionPoint`).
- **GUID text is canonical:** `Ext.StaticData.Get('Race',
  '0eb594cb-8820-4be6-a58d-8be7a1a98fba')` → `Human`, and its `ResourceUUID`
  round-trips to the same text. Before the fix the D/E groups were pair-swapped
  and the lookup returned nil.
- **CharacterCreationAppearanceVisual (#100):** 1315 entries; 1306 carry a
  `RaceUUID` and all 1306 resolve through `Get('Race', …)` (Dragonborn 313,
  Tiefling 182, HalfOrc 155, Human 137, Elf 132, Drow 132, HalfElf 127,
  Githyanki 32, Dwarf 30, Halfling 28, Gnome 27, Gnome_Deep 11). `SlotName`
  distribution: Hair 453, Private Parts 273, Head 232, DragonbornJaw 97,
  DragonbornChin 96, DragonbornTop 96, Horns 60, Tail 8. Entry 1
  `b24d2bbc-f40a-4c61-a8c5-6575d78be612`: `RootTemplate
  7d427a86-81f0-418d-a32a-3b5676c86bfa`, `VisualResource
  bf6ea9d0-db38-d44c-8cbd-12e6aca03044`, `DisplayName.Handle.Handle
  hccae5707gc982g44c4g837dgbad2da43e03f` → `Ext.Loca.GetTranslatedString` →
  **"Gale Hair"**. `Get(type, ResourceUUID)` round trip true. Unset
  `ArgumentString` handles surface the engine sentinel
  `ls::TranslatedStringRepository::s_HandleUnknown` verbatim.
- **#88 probe:** `cmake -B build` reports `Performing Test
  BG3SE_OBJCXX_TOOLCHAIN_OK - Success` on this machine (Xcode 26.2 SDK);
  `bg3se-harness doctor` adds five `toolchain_*` rows, all passing here.

### Release build (v0.44.0 dylib, 07:52 deploy, PID 60739)

- `!identity` → `version:"0.44.0"`, `game_state:"Running"`, `session_init:"complete"`.
- **Tier 1: 114/114. Tier 2: 112/114** — the same two failures as Phase 5
  (`Stats.DamageEvents.PairedFiring` environmental, `Wave7.Stats.AddEnumerationValue`
  gated). The four new StaticData tests pass, including `StaticData.AllTypesPopulated`
  (every bank populated, `GetCount == #GetAll`, ≥ 99% v4 GUIDs per type).
- Offline on the same commit: tier 0 141/141, harness pytest 368 passed.
- PID 56508 (first launch of this build) stalled at the menu; SIGTERM + relaunch loaded.

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

## Post-release review follow-ups (2026-09-15, PID 43783)

Four reviews of the v0.44.0 diff (Codex gpt-5.6-sol plus three scoped Claude
reviewers) produced the hardening recorded under `[Unreleased]` in
`docs/CHANGELOG.md`: bounded bank counts, GUID-array headers and hash-table
walks, bank revalidation at every SessionLoaded, stride candidates confirmed at
entry 2, `guid_parse` nibble validation, the CMake SDK detection moved before
`project()`, and a transactional installer. Rebuilt dylib verified live on
7398727 after one menu stall (PID 40630, SIGTERM + relaunch as before):
`!test_ingame StaticData` **6/6**, all 10 banks resolved with the same strides
as the release run (Feat 0x128, Race 0x168, Background 0x70, Origin 0x190,
God 0x60, Class 0x110, Progression 0x148, ActionResource 0x60,
FeatDescription 0x60, CCAV 0xA8), CCAV 1315 entries, 1306/1306 `RaceUUID`
resolutions, `Human` round trip, `Gale Hair` through `Ext.Loca`. Offline:
tier 0 142/142, tier H 368/368. The Origin and Progression configured
constants were still the old estimates (0x180, 0x200) and are now the live
values, so the stride-override log line no longer fires for them.
