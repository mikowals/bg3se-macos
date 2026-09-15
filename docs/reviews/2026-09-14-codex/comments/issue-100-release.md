# Issue #100 — v0.44.0 release comment (2026-09-15, Codex gpt-5.6-sol copy pass)

Shipped in v0.44.0: https://github.com/tdimino/bg3se-macos/releases/tag/v0.44.0

`Ext.StaticData.GetAll("CharacterCreationAppearanceVisual")` now returns the full bank—1315 entries on 4.1.1.7398727. Each includes `RootTemplate`, `RaceUUID`, `BodyType`, `BodyShape`, `SlotName`, `VisualResource`, `HeadAppearanceUUID`, `DefaultSkinColor`, `DisplayName` (a TranslatedString table; pass `DisplayName.Handle.Handle` to `Ext.Loca.GetTranslatedString`), `IconIdOverride`, `DefaultForBodyType`, `TextureEntryPart`, and `Tags`. All 1306 entries with a `RaceUUID` resolve through `Ext.StaticData.Get("Race", …)`. BG3SX's `SessionLoaded` handler no longer aborts on the unknown-type error.

This exposed a broader defect: on this game build, every StaticData type except `ActionResource` read a TypeId global instead of its resource bank. `GetAll` therefore returned empty or garbage tables, and GUID text had its last two groups pair-swapped. Both defects are fixed. Every type now resolves through the engine's headmaster table, with the entry stride measured live. `Ext.StaticData.Get("Race", "0eb594cb-8820-4be6-a58d-8be7a1a98fba")` returns `Human`.

`RequireFiles` remains a mod-side helper, not an extender builtin on either platform. If AbsoluteDefeat still fails on v0.44.0, post the defining file and full `latest.log`. I will trace the load order. Closing the StaticData issue; reopen with that log if the `RequireFiles` failure persists.
