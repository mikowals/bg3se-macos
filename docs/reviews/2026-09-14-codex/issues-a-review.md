I’m using the `bg3se-macos-ghidra` skill because this triage targets the macOS BG3SE port. I’ll keep the work strictly read-only, read all ten issue dumps in full, then trace each claim against current `main` and relevant history.
All ten dumps are read. The threads already identify several likely resolutions, but I’m treating those comments as hypotheses until current `HEAD`, version gates, path resolution, build checks, and overlay implementation corroborate them.
The code confirms the external-drive reports are fixed uniformly, not only in one launcher: the shell resolver, Python harness, dylib version detector, and deploy hook all accept `BG3SE_GAME_PATH` and scan Steam’s `libraryfolders.vdf`. I’m now separating “source-fixed” cases from reports that still require reporter-side confirmation or missing diagnostics.
I’ve finished correlating the ten threads with the current hook gates, loader behavior, path resolver, injection harness, and console implementation. I’m tightening the evidence now, especially where a thread’s apparent fix differs from what current main actually guarantees.
## Summary

Current main is `01b6b91cf0cdaf64b910eb7c63beb1f869270778`, with dylib `0.43.0` and verified BG3 build `4.1.1.7398727` ([version.h:9](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/core/version.h:9), [version_detect.h:25](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/core/version_detect.h:25)). One discrepancy in the supplied context: current CMake requests C23, C++20, and Objective-C++20—not C17 ([CMakeLists.txt:36](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/CMakeLists.txt:36)).

| Issue | Title | Reporter’s build | Classification | Action | Effort |
|---|---|---|---|---|---|
| #100 | Missing `RequireFiles` + unsupported StaticData type | BG3 7209685; SE 0.43.0 | **OPEN-NEEDS-FIX** | Implement both compatibility gaps; keep open | L |
| #94 | Deterministic hotbar SIGBUS | BG3 7209685; SE 48d1c97/v0.39.0 | **FIXED-ON-MAIN** | Close; reporter confirmed crash resolution | — |
| #92 | Hotfix 36 launch SIGBUS | BG3 7209685; SE v0.37.1 | **FIXED-ON-MAIN** | Close as resolved by guarded per-version offsets | — |
| #90 | External-drive game not found | Not stated | **FIXED-ON-MAIN** | Close; point to resolver/override | — |
| #89 | Crash to Desktop | BG3/SE not stated | **STALE-CLOSE** | Close; reporter reports local resolution | — |
| #88 | `'tuple' file not found` | Compile-time; M1 Pro/Sonoma 14.8.7 | **OPEN-NEEDS-FIX** | Add real toolchain preflight; request diagnostics | M |
| #86 | Alternate/external install directory | Compile-time; build not stated | **FIXED-ON-MAIN** | Close; same resolver as #90 | — |
| #84 | Injection marker not found | BG3 7209685; SE v0.37.1 | **FIXED-ON-MAIN** | Close marker bug; direct users to harness | — |
| #82 | Claimed arm64e requirement | BG3/SE not stated; macOS 15.7 | **NEEDS-INFO** | Reject arm64e diagnosis; request real failure artifacts | S triage |
| #80 | Overlay console non-functional | SE v0.36.50/f671848 | **OPEN-NEEDS-FIX** | Retire AppKit input path; retain socket workaround | L |

## Per-issue triage

### #100 — Missing global `RequireFiles` + unsupported StaticData type

**Report summary.** On BG3 `4.1.1.7209685` with SE `0.43.0`, AbsoluteDefeat’s client initialization failed because `RequireFiles` was nil, while BG3SX’s `SessionLoaded` callback failed on `CharacterCreationAppearanceVisual` ([issue-100.md:5](</private/tmp/claude-501/-Users-tomdimino-Desktop-Programming/game-modding/bg3/bg3se-macos/60cd31ce-019d-4c7c-9024-600e8edb88ff/scratchpad/pr-review/issues/issue-100.md:5), [issue-100.md:10](</private/tmp/claude-501/-Users-tomdimino-Desktop-Programming/game-modding/bg3/bg3se-macos/60cd31ce-019d-4c7c-9024-600e8edb88ff/scratchpad/pr-review/issues/issue-100.md:10)). There were no maintainer comments or conclusion.

**Current-source diagnosis.** Main implements `Ext.Require` and a mod-aware global `require`, but `register_global_functions()` exports only `_P`, `_D`, and `require`; there is no `RequireFiles` global ([main.c:817](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/injector/main.c:817), [main.c:830](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/injector/main.c:830), [main.c:2562](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/injector/main.c:2562)).

StaticData is a handwritten registry of nine types—Feat, Race, Background, Origin, God, Class, Progression, ActionResource, and FeatDescription. `CharacterCreationAppearanceVisual` is not registered ([staticdata_manager.h:35](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/staticdata/staticdata_manager.h:35), [staticdata_manager.c:91](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/staticdata/staticdata_manager.c:91), [staticdata_manager.c:116](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/staticdata/staticdata_manager.c:116)). The current tree has no `staticdata_registry.c` or `generated_staticdata_registry.c`; registration remains centralized in `staticdata_manager.[ch]`.

An unknown type is a hard Lua error in `GetAll`, `Get`, and `GetCount`; it is not warn-and-continue ([lua_staticdata.c:75](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/lua/lua_staticdata.c:75), [lua_staticdata.c:120](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/lua/lua_staticdata.c:120), [lua_staticdata.c:154](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/lua/lua_staticdata.c:154)). This is not a native process crash: bootstrap execution is protected by `lua_pcall`, and event dispatch logs a failing handler before continuing to later handlers ([main.c:725](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/injector/main.c:725), [lua_events.c:348](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/lua/lua_events.c:348)). It nevertheless aborts the affected bootstrap or event handler and breaks those mods.

**Disposition: OPEN-NEEDS-FIX, L.** Add a Windows-compatible `RequireFiles` helper beside the existing require machinery and register it globally, with path/order/result tests. Add `CharacterCreationAppearanceVisual` through manager discovery, entry layout/proxy serialization, and compatibility tests; keep genuinely invalid type names as errors, or introduce a deliberate known-but-unimplemented warning policy rather than silently accepting every typo. The Windows reference identifies the corresponding engine class as `eoc::CharacterCreationAppearanceVisualManager` ([GuidResources.h:702](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se/BG3Extender/GameDefinitions/GuidResources.h:702)).

### #94 — Deterministic SIGBUS in hotbar `FinalizeAddSlot`

**Report summary.** BG3 `7209685` crashed reproducibly during a new game with zero mods, on both `48d1c97` and v0.39.0 ([issue-94.md:7](</private/tmp/claude-501/-Users-tomdimino-Desktop-Programming/game-modding/bg3/bg3se-macos/60cd31ce-019d-4c7c-9024-600e8edb88ff/scratchpad/pr-review/issues/issue-94.md:7)). The thread traced it to stale staticdata/template code patches admitted by overly weak sentinel checks; after `11f9d6d`, the reporter confirmed that the crash was resolved and the offset audit passed ([issue-94.md:33](</private/tmp/claude-501/-Users-tomdimino-Desktop-Programming/game-modding/bg3/bg3se-macos/60cd31ce-019d-4c7c-9024-600e8edb88ff/scratchpad/pr-review/issues/issue-94.md:33), [issue-94.md:45](</private/tmp/claude-501/-Users-tomdimino-Desktop-Programming/game-modding/bg3/bg3se-macos/60cd31ce-019d-4c7c-9024-600e8edb88ff/scratchpad/pr-review/issues/issue-94.md:45)).

**Current-source diagnosis.** StaticData code-patch installers now require `version_detect_matches()`, even when an older version has an offset-table row ([staticdata_manager.c:688](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/staticdata/staticdata_manager.c:688), [staticdata_manager.c:980](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/staticdata/staticdata_manager.c:980)). Template capture no longer patches code at all; it reads per-version singleton pointers and skips when no exact offset-table row exists ([template_manager.c:249](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/template/template_manager.c:249), [template_manager.c:442](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/template/template_manager.c:442)). Functor hooks independently require `FUNCTOR_ADDRS_VERIFIED_BUILD`, currently `7398727` ([main.c:4420](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/injector/main.c:4420), [functor_types.h:373](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/stats/functor_types.h:373)).

Thus the specific stale-code-patch mechanism cannot install on an unverified build now. `7398727` is today’s verified exact-build gate, and nm-based tests verify table entries against exact symbols ([version_detect.c:247](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/core/version_detect.c:247), [test_offset_audit.py:283](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/tests/harness/test_offset_audit.py:283)).

**Disposition: FIXED-ON-MAIN.** Close. The later clean session-initialization failure was separate, and the reporter’s attached launch log used the now-retired direct `DYLD_INSERT_LIBRARIES` path.

### #92 — Hotfix 36 launch SIGBUS

**Report summary.** v0.37.1 detected BG3 `7209685` as a mismatch against `6995620`, but all three data sentinels passed and enabled stale address-dependent features immediately before a SIGBUS ([issue-92.md:9](</private/tmp/claude-501/-Users-tomdimino-Desktop-Programming/game-modding/bg3/bg3se-macos/60cd31ce-019d-4c7c-9024-600e8edb88ff/scratchpad/pr-review/issues/issue-92.md:9), [issue-92.md:22](</private/tmp/claude-501/-Users-tomdimino-Desktop-Programming/game-modding/bg3/bg3se-macos/60cd31ce-019d-4c7c-9024-600e8edb88ff/scratchpad/pr-review/issues/issue-92.md:22)). The maintainer identified the same false-positive stale-patch cause as #94 and marked it fixed by `11f9d6d`; no reporter retest followed ([issue-92.md:47](</private/tmp/claude-501/-Users-tomdimino-Desktop-Programming/game-modding/bg3/bg3se-macos/60cd31ce-019d-4c7c-9024-600e8edb88ff/scratchpad/pr-review/issues/issue-92.md:47)).

**Current-source diagnosis.** The broad address-safety check still permits sentinel probing on version mismatches, but exact version detection drives the offset-table row, and unknown versions get no active table ([version_detect.c:319](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/core/version_detect.c:319), [offset_table.c:345](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/core/offset_table.c:345)). More importantly, the implicated StaticData patch families have a second exact-version gate, template capture has no code hooks, and functors use their own exact ABI gate. Therefore a sentinel false positive alone cannot recreate this stale StaticData/template patch crash on current main.

**Disposition: FIXED-ON-MAIN.** Close as source-resolved by `e27bcd8`/`11f9d6d` and superseded by the `7398727` migration in `01b6b91`. No additional confirmation is needed to establish that this specific unsafe install path is gone.

### #90 — External-drive game not found

**Report summary.** The installer searched only the default internal Steam directory while BG3 lived on a LaCie external drive. A symlink worked around it, then the maintainer marked it fixed in `684120c` through a shared override/default/VDF resolution order ([issue-90.md:5](</private/tmp/claude-501/-Users-tomdimino-Desktop-Programming/game-modding/bg3/bg3se-macos/60cd31ce-019d-4c7c-9024-600e8edb88ff/scratchpad/pr-review/issues/issue-90.md:5), [issue-90.md:16](</private/tmp/claude-501/-Users-tomdimino-Desktop-Programming/game-modding/bg3/bg3se-macos/60cd31ce-019d-4c7c-9024-600e8edb88ff/scratchpad/pr-review/issues/issue-90.md:16)).

**Current-source diagnosis.** The Python harness accepts `BG3SE_GAME_PATH`, then checks the default Steam library and all roots parsed from `libraryfolders.vdf` ([config.py:13](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/tools/bg3se_harness/config.py:13)). The shell resolver and injected dylib implement the same order ([find_bg3.sh:5](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/scripts/find_bg3.sh:5), [version_detect.c:134](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/core/version_detect.c:134)). No symlink is required.

**Disposition: FIXED-ON-MAIN.** Close. For a non-Steam or unusual layout, set `BG3SE_GAME_PATH` to either the `.app` bundle or its containing directory.

### #89 — Crash to Desktop

**Report summary.** The report supplied only macOS 15.7.7 and an M4 Mac Studio, with no BG3 version, SE revision, log, or crash report ([issue-89.md:5](</private/tmp/claude-501/-Users-tomdimino-Desktop-Programming/game-modding/bg3/bg3se-macos/60cd31ce-019d-4c7c-9024-600e8edb88ff/scratchpad/pr-review/issues/issue-89.md:5)). The maintainer requested those artifacts and suggested the then-current stale-offset class; the reporter later said a delete/restart/reinstall restored normal operation ([issue-89.md:10](</private/tmp/claude-501/-Users-tomdimino-Desktop-Programming/game-modding/bg3/bg3se-macos/60cd31ce-019d-4c7c-9024-600e8edb88ff/scratchpad/pr-review/issues/issue-89.md:10), [issue-89.md:21](</private/tmp/claude-501/-Users-tomdimino-Desktop-Programming/game-modding/bg3/bg3se-macos/60cd31ce-019d-4c7c-9024-600e8edb88ff/scratchpad/pr-review/issues/issue-89.md:21)).

**Current-source diagnosis.** There is insufficient evidence to attribute this crash to any current subsystem. If it was the `7209685` stale-patch class, the exact-version gates described under #94/#92 address it; otherwise the absence of `latest.log`, `.ips`, game version, and SE commit prevents further diagnosis.

**Disposition: STALE-CLOSE.** The reporter explicitly confirmed local resolution, and no unresolved reproducible defect remains.

### #88 — `'tuple' file not found`

**Report summary.** A fresh M1 Pro/Sonoma 14.8.7 build failed while compiling the Objective-C++ Metal backend because the SDK’s simd header could not find the libc++ `<tuple>` header ([issue-88.md:7](</private/tmp/claude-501/-Users-tomdimino-Desktop-Programming/game-modding/bg3/bg3se-macos/60cd31ce-019d-4c7c-9024-600e8edb88ff/scratchpad/pr-review/issues/issue-88.md:7), [issue-88.md:20](</private/tmp/claude-501/-Users-tomdimino-Desktop-Programming/game-modding/bg3/bg3se-macos/60cd31ce-019d-4c7c-9024-600e8edb88ff/scratchpad/pr-review/issues/issue-88.md:20)). The maintainer recommended a clean configure, explicit sysroot, or full Xcode, but there was no confirmation.

**Current-source diagnosis.** This is a selected-toolchain/sysroot/libc++ mismatch, not a missing C++20 setting: CMake requires C++20 and Objective-C++20, and the failing Metal backend is part of the dylib target ([CMakeLists.txt:36](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/CMakeLists.txt:36), [CMakeLists.txt:242](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/CMakeLists.txt:242)). Main does run `xcrun --sdk macosx --show-sdk-path` with a CommandLineTools fallback, and troubleshooting documents the manual/full-Xcode recovery ([CMakeLists.txt:4](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/CMakeLists.txt:4), [troubleshooting.md:39](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/docs/troubleshooting.md:39)).

However, the thread’s assertion that SDK auto-detection landed after the report is incorrect: `git blame` attributes that block to `720d067`/`4f61f12` on 2026-03-31, while the report dates to 2026-05-22. The purported fix therefore already existed on the reporter’s fresh clone. Additionally, harness documentation says `doctor` verifies the SDK, but the implementation begins with game/dylib/patch/tool checks and reaches its summary without any `xcrun`, compiler, SDK, or `<tuple>` probe ([harness.md:56](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/docs/harness.md:56), [doctor.py:45](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/tools/bg3se_harness/doctor.py:45), [doctor.py:303](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/tools/bg3se_harness/doctor.py:303)).

**Disposition: OPEN-NEEDS-FIX, M.** Add a configure-time Objective-C++ compile probe importing MetalKit and `<tuple>`, failing with selected compiler/sysroot diagnostics. Add matching `doctor` checks for `xcode-select -p`, `xcrun --show-sdk-path`, compiler version, libc++ header visibility, and correct the documentation. Request the reporter’s selected developer directory, `xcrun` result, compiler version, and relevant CMake cache values.

### #86 — Alternate source directory/external SSD

**Report summary.** The build’s post-build deploy step failed because BG3 was on `/Volumes/Extra Extra/...` rather than the default Steam library, causing CMake to delete the otherwise-built dylib ([issue-86.md:5](</private/tmp/claude-501/-Users-tomdimino-Desktop-Programming/game-modding/bg3/bg3se-macos/60cd31ce-019d-4c7c-9024-600e8edb88ff/scratchpad/pr-review/issues/issue-86.md:5)). The maintainer marked it fixed by the same resolver introduced for #90 ([issue-86.md:27](</private/tmp/claude-501/-Users-tomdimino-Desktop-Programming/game-modding/bg3/bg3se-macos/60cd31ce-019d-4c7c-9024-600e8edb88ff/scratchpad/pr-review/issues/issue-86.md:27)).

**Current-source diagnosis.** CMake still invokes `scripts/deploy.sh` post-build, but that script now uses the shared resolver and exits successfully with a warning when no game is found ([CMakeLists.txt:337](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/CMakeLists.txt:337), [deploy.sh:8](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/scripts/deploy.sh:8), [deploy.sh:15](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/scripts/deploy.sh:15)). External Steam libraries are read from `libraryfolders.vdf`, while `BG3SE_GAME_PATH` covers arbitrary locations.

**Disposition: FIXED-ON-MAIN.** Close, citing `684120c`. The commenter’s Documents path is BG3’s user-data directory, not the application bundle, so it is not a valid deployment target.

### #84 — Injection marker not found

**Report summary.** `launch_bg3.sh` warned that its injection marker was missing even though the attached session log showed SE v0.37.1 initialized inside BG3, detected the enabled cosmetic mods, and reported no SE mods ([issue-84.md:26](</private/tmp/claude-501/-Users-tomdimino-Desktop-Programming/game-modding/bg3/bg3se-macos/60cd31ce-019d-4c7c-9024-600e8edb88ff/scratchpad/pr-review/issues/issue-84.md:26), [issue-84.md:48](</private/tmp/claude-501/-Users-tomdimino-Desktop-Programming/game-modding/bg3/bg3se-macos/60cd31ce-019d-4c7c-9024-600e8edb88ff/scratchpad/pr-review/issues/issue-84.md:48), [issue-84.md:61](</private/tmp/claude-501/-Users-tomdimino-Desktop-Programming/game-modding/bg3/bg3se-macos/60cd31ce-019d-4c7c-9024-600e8edb88ff/scratchpad/pr-review/issues/issue-84.md:61)). The thread concluded that the script checked a file the dylib never wrote; `8aba568` changed it to validate a fresh real session log, while cosmetic PAK loading remained a game/modsettings concern.

**Current-source diagnosis.** The legacy script now compares `logs/latest.log` against its launch timestamp rather than looking for a marker ([launch_bg3.sh:49](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/scripts/launch_bg3.sh:49), [CHANGELOG.md:810](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/docs/CHANGELOG.md:810)). Current supported injection is instead the harness: `patch` adds a weak dylib load command, ad-hoc signs, and verifies the linkage using `otool` ([patch.py:87](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/tools/bg3se_harness/patch.py:87), [harness.md:134](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/docs/harness.md:134)). `status` reports process/socket/patch state, and `doctor` verifies the binary patch and `insert_dylib` availability ([cli.py:558](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/tools/bg3se_harness/cli.py:558), [doctor.py:84](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/tools/bg3se_harness/doctor.py:84)).

**Disposition: FIXED-ON-MAIN.** Close the false-marker issue and direct future reports to `PYTHONPATH=tools python3 -m bg3se_harness doctor`, `status`, and `patch`. The old `launch_bg3.sh` still uses the deprecated DYLD path and should not be presented as the current launch method ([architecture.md:29](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/docs/architecture.md:29)).

### #82 — Claimed arm64e requirement

**Report summary.** On macOS 15.7 Apple Silicon, an AI assistant told the reporter that BG3 required arm64e while Dobby only produced arm64; no command output, game version, SE revision, log, or crash report was supplied ([issue-82.md:5](</private/tmp/claude-501/-Users-tomdimino-Desktop-Programming/game-modding/bg3/bg3se-macos/60cd31ce-019d-4c7c-9024-600e8edb88ff/scratchpad/pr-review/issues/issue-82.md:5)). The maintainer rejected that diagnosis and requested a current session log if the problem persisted.

**Current-source diagnosis.** The arm64e claim is incorrect. BG3’s Steam binary is documented and handled as a universal `x86_64 + arm64` binary, and the project builds exactly those two slices ([reverse-engineering.md:69](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/docs/reverse-engineering.md:69), [CMakeLists.txt:44](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/CMakeLists.txt:44)). Harness verification explicitly requires both `arm64` and `x86_64` in the dylib ([build.py:26](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/tools/bg3se_harness/build.py:26)). Current static patching also strips the old environment-injection failure mode and re-signs after mutation.

**Disposition: NEEDS-INFO, S triage.** Ask for BG3 and SE versions, the exact failed command and complete output, harness `doctor` and `status` JSON, `file` output for both executable and dylib, and `latest.log` or `.ips` depending on whether initialization never occurs or crashes. Do not pursue arm64e or change Dobby architecture flags.

### #80 — Overlay console still non-functional

**Report summary.** On v0.36.50, the reporter isolated queue drainage, hidden errors, silent Osiris failures, `NSTextField`/TSM crashes, a `__retain_OA` submit crash, double polling, a main-queue deadlock, and listen-only input; the socket console remained the only reliable interface ([issue-80.md:8](</private/tmp/claude-501/-Users-tomdimino-Desktop-Programming/game-modding/bg3/bg3se-macos/60cd31ce-019d-4c7c-9024-600e8edb88ff/scratchpad/pr-review/issues/issue-80.md:8), [issue-80.md:16](</private/tmp/claude-501/-Users-tomdimino-Desktop-Programming/game-modding/bg3/bg3se-macos/60cd31ce-019d-4c7c-9024-600e8edb88ff/scratchpad/pr-review/issues/issue-80.md:16)). The maintainer said v0.38 fixed the queue/Lua serialization/output layer but deliberately did not replace the dangerous AppKit input architecture.

**Current-source diagnosis.** Queue drainage and output ordering are fixed: overlay commands are mutex-protected and drained by `console_poll`, while output is batched through one ordered main-queue stream ([console.c:83](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/console/console.c:83), [console.c:853](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/console/console.c:853), [overlay.m:658](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/overlay/overlay.m:658)). TextKit 1 also addresses the later output-flood SIGBUS, not the input TSM crash ([overlay.m:411](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/overlay/overlay.m:411), [CHANGELOG.md:856](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/docs/CHANGELOG.md:856)).

The central blockers remain:

- The console still uses an editable `NSTextField`, becomes key, and calls `makeKeyAndOrderFront`, preserving the reported TSM surface ([overlay.m:299](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/overlay/overlay.m:299), [overlay.m:742](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/overlay/overlay.m:742), [overlay.m:852](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/overlay/overlay.m:852), [overlay.m:898](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/overlay/overlay.m:898)).
- `overlay_is_visible()` still performs an unconditional synchronous dispatch to the main queue and can deadlock when called there ([overlay.m:936](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/overlay/overlay.m:936)).
- The CGEvent tap is still listen-only and explicitly cannot consume input ([input_hooks.m:240](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/input/input_hooks.m:240), [input_hooks.m:341](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/input/input_hooks.m:341)).
- Overlay commands use client slot `-1`, but several error paths still call `console_error()` only for slots `>=0`, so Lua errors remain absent from the overlay ([console.c:602](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/console/console.c:602), [console.c:665](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/console/console.c:665)).
- Missing Osiris functions or dispatchers still return nil with log-only feedback ([main.c:2054](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/injector/main.c:2054), [main.c:2261](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/injector/main.c:2261)).
- Mods and Entities remain placeholder displays ([overlay.m:445](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/overlay/overlay.m:445), [overlay.m:564](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/overlay/overlay.m:564)).

F11 toggles the separate ImGui debug overlay; the AppKit console uses Ctrl+grave ([imgui_metal_backend.mm:1489](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/imgui/imgui_metal_backend.mm:1489), [main.c:2888](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/src/injector/main.c:2888)). Historically, #65 covered Enter/session initialization, while #66 corrected unsafe Osiris dispatch structures and encoded handles ([CHANGELOG.md:1522](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/docs/CHANGELOG.md:1522), [CHANGELOG.md:1483](/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos/docs/CHANGELOG.md:1483)).

**Disposition: OPEN-NEEDS-FIX, L.** Retire the editable AppKit control and either fold the console into ImGui or implement a pure event-tap/C input buffer. Make visibility state main-thread-aware or atomic, route overlay and Osiris errors visibly, make initialization one-time/thread-safe, add submit/history/toggle tests at menu and in-session, and either wire the placeholder tabs to live data or remove them. Keep the socket console as the supported workaround.

## Draft maintainer comments

### #100

Confirmed on current main. We export `Ext.Require` and global `require`, but not `RequireFiles`. StaticData exposes only nine resource types and rejects `CharacterCreationAppearanceVisual` with a Lua error. These are protected Lua failures rather than native crashes, but they abort the affected bootstrap or event handler and break both mods.

Keeping this open. The fix requires a compatible `RequireFiles` helper plus manager discovery, entry serialization, and tests for `CharacterCreationAppearanceVisual`. Unknown misspellings will remain errors; known compatibility types should be explicitly implemented or reported as unsupported without taking down unrelated handlers.

### #94

Confirmed fixed on main. The stale StaticData code-patch path now requires an exact verified game version, template capture no longer installs code hooks, and functor patches use their own exact ABI gate. Current verification targets BG3 `4.1.1.7398727`, with nm-backed offset auditing.

Your retest confirmed that the hotbar SIGBUS was resolved. The later clean session-initialization failure came from a separate obsolete DYLD launch path. Closing this issue.

### #92

Fixed on main. The three readable data sentinels can no longer authorize the implicated stale StaticData/template code patches: StaticData patch installers require the exact verified build, template capture is pointer-read-only, and functor patches use an independent ABI gate.

Current main targets BG3 `4.1.1.7398727`; unknown builds fail closed or run without those hooks. Closing this Hotfix 36 stale-offset crash.

### #90

Fixed in `684120c` and present on current main. The harness, shell scripts, and dylib resolve BG3 through `BG3SE_GAME_PATH`, the default Steam library, then every library in `steamapps/libraryfolders.vdf`.

External-drive Steam libraries now work without a symlink. For an unusual layout, set `BG3SE_GAME_PATH` to the BG3 `.app` bundle or its containing directory. Closing.

### #89

The reporter confirmed that deleting, restarting, and reinstalling restored normal operation. No BG3 version, Script Extender revision, session log, or crash report was supplied, so no remaining reproducible repository defect can be identified.

Closing as resolved locally. A new crash should be filed with `latest.log`, the newest BG3 `.ips` report, the BG3 version, and the exact SE commit.

### #88

This remains open. The SDK auto-detection cited in the previous reply already existed before this report, so it does not establish a fix. Current `doctor` also does not inspect the selected developer directory, SDK, compiler, or libc++ headers despite the harness documentation saying it verifies the SDK.

Please post `xcode-select -p`, `xcrun --sdk macosx --show-sdk-path`, `clang++ --version`, and the `CMAKE_OSX_SYSROOT`/compiler entries from `build/CMakeCache.txt`. The repository needs a configure-time `<tuple>`/MetalKit compile probe and matching doctor diagnostics.

### #86

Fixed in `684120c`. The post-build deploy step now uses the shared game resolver, including external Steam libraries from `libraryfolders.vdf` and the `BG3SE_GAME_PATH` override. If BG3 is absent, deployment warns and skips instead of failing the build.

Set `BG3SE_GAME_PATH` to the application bundle or its containing directory for a nonstandard layout. The Documents/Larian Studios directory is user data, not the game application. Closing.

### #84

The injection-marker bug is fixed in `8aba568`: the old script checked a file the dylib never created, while your session log already proved successful injection. Current main verifies the real session log.

Use the harness’s `doctor`, `status`, and `patch` commands; static `insert_dylib` patching is now the supported injection path. The listed cosmetic PAKs are loaded by BG3 through `modsettings.lsx`, not by Script Extender. Closing this marker issue.

### #82

The arm64e diagnosis is incorrect. BG3 and the extender use ordinary arm64, with x86_64 included for the universal binary; no arm64e Dobby build is required.

The actual failure remains unidentified. Please provide the BG3 version, exact SE commit, failed command and complete output, harness `doctor` and `status` JSON, `file` output for the BG3 executable and dylib, and `latest.log` or the newest `.ips` report. Those artifacts will distinguish build, patch, signing, launch, and runtime failures.

### #80

This remains open. Current main fixed command-queue drainage, Lua serialization, ordered output, and the later TextKit output-flood crash. It still uses an editable `NSTextField`, makes the overlay key, synchronously queries the main queue, suppresses several overlay errors, and provides log-only feedback for Osiris dispatch failures. The Mods and Entities tabs remain placeholders.

The input path must move out of AppKit—preferably into the existing ImGui backend—before the overlay console is supportable. Use the socket console in the meantime. Keeping this issue open as the tracking issue for that rework.

No files were modified.
