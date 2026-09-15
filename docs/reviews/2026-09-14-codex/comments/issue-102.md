Credit first: @mageweaver and @marcus-sa shipped fixes and builds while this repo went quiet, and the people in this thread got a working extender out of it. The release gap is ours—no release since v0.39.0 in April while main moved to 4.1.1.7398727 in source and never cut a zip. That vacuum, not a regression on main, drove every "which build do I install" question above.

What main does next, in order:

1. Land #101 (PersistentVars saved as `null`, `Ext.Enums.X[n]` always nil, net hook installed at target+4, Osiris return values discarded) and #103.
2. Run live verification on the current Steam build.
3. Ship v0.44.0 with a prebuilt zip, the harness install one-liner, and the verified game build in the release notes.

Until v0.44.0 is out, treat the fork releases as working builds, and report their failures to the corresponding repository with exact extender, manager, mod and game versions. This thread is not the place for those.

@marcus-sa, @mageweaver: focused PRs with a reproducer and a tier-0 or compat test land fast. I want the resource-bank index off-by-one, Vars table identity, SpellMeta stride, and DB/PROC listener fixes first, each backed by its own ARM64 evidence. A 317-commit branch will not be bulk-cherry-picked.

Leaving this open until the release link is posted here.
