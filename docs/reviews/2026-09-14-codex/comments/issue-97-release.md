# Issue #97 — v0.44.0 release comment (2026-09-15, Codex gpt-5.6-sol copy pass)

Status 2026-09-15: v0.44.0 is released (https://github.com/tdimino/bg3se-macos/releases/tag/v0.44.0), built and verified on 4.1.1.7398727. Mod Configuration Menu passed 27/27 scenarios again on that build, with no regression against the July baseline (`docs/compat-reports/mod_configuration_menu_1789446019.json`).

The release pass found that `Ext.StaticData.GetAll` returned empty or garbage tables for every type except `ActionResource` on this game build, and GUID text was pair-swapped. The four rostered mods using StaticData—5e Spells, Expansion - Level 20, and anything reading Race/Class/Progression—were baselined green because their scenarios never asserted on StaticData values. v0.44.0 fixes the accessors. Those scenarios will receive value-level StaticData assertions and a re-run before the next baseline refresh.

Next: BG3SX (#98, now that `CharacterCreationAppearanceVisual` exists), BG3AF, Demon Hunter Class, Twin Blades. Nominations remain welcome below.
