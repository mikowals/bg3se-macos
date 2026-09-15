# v0.44.0 release notes (as published)

## First release on Baldur's Gate 3 4.1.1.7398727

Verified live on the current Steam build (4.1.1.7398727, arm64 `LC_UUID 0C51CAED-6D60-3DCD-9299-8519C92631B0`) on 2026-09-15: tier 1 `!test` **114/114**, tier 2 `!test_ingame` **112/114** (the two remaining failures are the environmental damage-functor probe and the ValueList insert, which stays gated on this build), Mod Configuration Menu compat scenario **27/27** with no regression against the July baseline. Offline: 141 C unit tests and 368 harness tests, 737 tests across four tiers.

This is the first release since v0.39.0 (April). Everything main accumulated since then ships here: the offset migration to 7398727, the dual-VM state-ownership work, the #101/#103 integration, and the fixes the live pass found.

### Install (prebuilt dylib)

Quit the game, then:
```bash
unzip bg3se-macos-v0.44.0-universal.zip && cd bg3se-macos-v0.44.0 && ./install.sh
```
`install.sh` copies `libbg3se.dylib` into `Baldur's Gate 3.app/Contents/MacOS/`, backs up the game binary as `Baldur's Gate 3.bg3se-original`, adds the `LC_LOAD_WEAK_DYLIB` load command with the bundled `insert_dylib` (static Mach-O patching; `DYLD_INSERT_LIBRARIES` does not survive Steam), and ad-hoc re-signs. It finds the game in any Steam library or through `BG3SE_GAME_PATH`. `./install.sh --uninstall` restores the original binary. After a game update, run it again. Apple Silicon only for the installer (the bundled patcher is arm64); Intel Macs build from source.

From a checkout instead: `cmake -B build && cmake --build build` builds, deploys and patches, and `PYTHONPATH=tools python3 -m bg3se_harness doctor` verifies the install, the patch state, and the build toolchain.

Launch through Steam as usual. Logs land in `~/Library/Application Support/BG3SE/logs/latest.log`; the console is `nc -U /tmp/bg3se.sock`.

### Fixed

- **PersistentVars saved as `null`** for every mod (#101, @mikowals).
- **`Ext.Enums.X[n]` always nil**, extended to enum equality and bitfield operands (#101, @mikowals).
- **ARM64 safe hooks installed at target+4**; dirty entry windows now fail closed, `TBNZ` recognised (#101, @mikowals; #106).
- **Osiris wrappers discarded the engine's return value** (#101, @mikowals).
- **`Ext.Json.Stringify` on component and entity proxies returned `"null"`**; proxies serialize in place with a cycle guard.
- **`Ext.StaticData` returned empty or garbage tables** for most types: the accessors read a TypeId global instead of the resource bank. Every type now resolves its bank through the engine's headmaster table with the entry stride measured live (Race, Background, Origin, Class, Progression, ActionResource, FeatDescription all had wrong strides). GUID text was also pair-swapped in the last two groups; `Ext.StaticData.Get("Race", "0eb594cb-8820-4be6-a58d-8be7a1a98fba")` now returns `Human`.
- **`SpellContainer.Spells` stride** 80 → 96 (the mageweaver fork's finding was correct; #102).
- **`entity.Uuid` nil**, **component GUIDs byte-reversed**, **`Transform.Translate` nil**, **`GetAllEntitiesWithUuid()` empty**: all found and fixed live on 7398727.

### Added

- **`Ext.StaticData` type `CharacterCreationAppearanceVisual`** (#100) with the full Windows property surface (1315 entries live; all 1306 `RaceUUID`s resolve to Race entries; `DisplayName` handles resolve through `Ext.Loca`).
- **Configure-time Objective-C++ toolchain probe** (#77, #88): `cmake` fails early with `xcode-select -p`, the SDK path, `CMAKE_OSX_SYSROOT`, the compiler, and where `<tuple>` lives, instead of dying at 57% of the build. `bg3se-harness doctor` runs the same checks.
- 23 tier-0 mutation-hardening tests (#103, @mikowals) and 73 further tier-0/tier-H/tier-2 tests across the integration and live passes.

### Credits

@mikowals for #101 and #103. @mageweaver and @marcus-sa for the fork findings that went onto the live probe list. Reporters on #88, #99, #100, #102, #106 for the evidence.

Full detail: [CHANGELOG](https://github.com/tdimino/bg3se-macos/blob/main/docs/CHANGELOG.md), [live verification record](https://github.com/tdimino/bg3se-macos/blob/main/docs/parity-100/LIVE-VERIFICATION-2026-09-14.md).
