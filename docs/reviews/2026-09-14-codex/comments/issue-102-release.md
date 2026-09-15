# Issue #102 — v0.44.0 release comment (2026-09-15, Codex gpt-5.6-sol copy pass)

v0.44.0 is out: https://github.com/tdimino/bg3se-macos/releases/tag/v0.44.0

Built and verified on BG3 4.1.1.7398727 (the current Steam build). The zip contains the universal dylib. The release notes include installation steps and the harness one-liner.

Since my last comment, #101 and #103 landed with credit to @mikowals, along with the live verification pass, the SpellMeta stride, four other value-level fixes found live, StaticData bank resolution (every type returned empty or garbage tables on this build), the `CharacterCreationAppearanceVisual` type for #100, and the toolchain probe for #88.

Credit again to @mageweaver and @marcus-sa for the fork findings. The SpellMeta stride claim was correct and is included. Focused PRs with a reproducer and a tier-0 or compat test remain the fast path. Closing this thread against the release. Open a new issue for failures on v0.44.0 with exact extender, mod, and game versions.
