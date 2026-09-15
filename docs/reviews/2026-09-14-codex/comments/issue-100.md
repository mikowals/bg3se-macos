Two different things here.

`RequireFiles` is not an extender builtin—not on macOS, and not in Norbyte's Windows reference either. The mod's own bootstrap defines this helper, so a missing global means an earlier file in its load sequence failed. Post that definition and the full `latest.log`, and I can identify the failure.

The StaticData half is a real gap on main: only nine types are registered (`src/staticdata/staticdata_manager.h`) and `CharacterCreationAppearanceVisual` is not among them, so the lookup raises a Lua error that aborts the mod's `SessionLoaded` handler. Adding the type (`eoc::CharacterCreationAppearanceVisualManager` on the Windows side) through manager discovery, layout and tests is queued for v0.44.0. Keeping open.
