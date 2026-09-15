BG3SX has not been vetted on main, so this stays open as a compat target. MCM is verified, not the blocker. The first main-side gap to test is story integration: native Osiris calls work, but Osiris database and PROC listeners are not live-verified, matching "NPC discovery or scene start never advances."

The acceptance test will be a `bg3sx` harness scenario asserting eligible-NPC discovery and scene startup. Post the exact BG3SX and BG3AF versions and a clean load order so the scenario matches what you run. Queued after v0.44.0.
