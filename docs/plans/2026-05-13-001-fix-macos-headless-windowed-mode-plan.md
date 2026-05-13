# Make macOS Headless Launch Use Windowed Mode

This ExecPlan is a living document. Sections Progress, Surprises &
Discoveries, Decision Log, and Outcomes & Retrospective must be
kept up to date as work proceeds.

## Purpose / Big Picture

The BG3SE harness should support `PYTHONPATH=tools python3 -m bg3se_harness launch --headless --background` without leaving the user trapped in a Baldur's Gate 3 fullscreen Space. A Space is a macOS virtual desktop. A fullscreen Space is the separate desktop macOS creates for an app that has entered native fullscreen. AppleScript can hide a normal windowed app, but it cannot reliably pull the user out of a game-owned fullscreen Space while BG3 is still creating Metal and Bink video windows.

The implementation should force BG3 into a normal window before launch by temporarily editing `~/Documents/Larian Studios/Baldur's Gate 3/graphicSettings.lsx`. After BG3 has read that file and the harness has hidden the windowed app, the harness should restore the user's original graphics settings so the next manual BG3 launch behaves the way the user configured it. The visible proof is that the background launch returns immediately, `/tmp/bg3se_monitor.log` reports repeated hide attempts and socket readiness, `System Events` reports the BG3 process is not visible, and the user's `graphicSettings.lsx` no longer contains the temporary headless windowed values after the hide step completes.

## Progress

- [x] (2026-05-13 16:22Z) Read `tools/bg3se_harness/launch.py`, including `launch()`, `hide_window()`, `show_window()`, and `_upsert_graphic_settings()`.
- [x] (2026-05-13 16:22Z) Read `tools/bg3se_harness/config.py` and confirmed `GRAPHIC_SETTINGS_PATH` points at the Larian Documents profile path.
- [x] (2026-05-13 16:22Z) Read `tools/bg3se_harness/_monitor.py` and confirmed background mode already has a persistent hide thread during boot.
- [x] (2026-05-13 16:22Z) Read `tools/bg3se_harness/cli.py` and confirmed `cmd_launch()` passes `--headless` and `--background` state to the monitor but does not currently pass headless state into `launch()`.
- [x] (2026-05-13 16:22Z) Inspected the local `graphicSettings.lsx` and confirmed it currently has `ScreenWidth` and `ScreenHeight`, but no `Fullscreen`, `FakeFullscreenEnabled`, `FakeFullscreen`, `WindowedMode`, or `FullscreenMode` entries.
- [x] (2026-05-13 16:22Z) Checked BG3 binary strings and confirmed `Fullscreen`, `FakeFullscreenEnabled`, `WindowedMode`, `Windowed`, `FullScreen`, `FakeFullScreen`, `ScreenWidth`, and `ScreenHeight` appear in the executable.
- [x] (2026-05-13 16:22Z) Checked public Larian and Steam discussion evidence for `graphicSettings.lsx` display-mode keys.
- [x] (2026-05-13 16:22Z) Drafted this implementation plan.
- [x] (2026-05-13 17:16Z) Implement the settings snapshot, temporary windowed write, and restore helpers.
- [x] (2026-05-13 17:16Z) Wire headless graphics preparation into `launch()` before BG3 starts.
- [x] (2026-05-13 17:16Z) Update `hide_window()`, `show_window()`, and `_monitor.py` around early hide, minimize, and restore timing.
- [x] (2026-05-13 17:16Z) Add focused offline tests for graphics-file mutation and CLI call order.
- [ ] Run live validation on macOS with `--headless --background`.
- [x] (2026-05-13 17:16Z) Record offline validation outcomes in this document.

## Surprises & Discoveries

Observation: The local BG3 graphics file has no display-mode key at all, even though BG3 is currently launching fullscreen.
Evidence: `~/Documents/Larian Studios/Baldur's Gate 3/graphicSettings.lsx` contains `ScreenWidth=1920` and `ScreenHeight=1080`, but the inspected file has no `Fullscreen`, `FakeFullscreenEnabled`, `FakeFullscreen`, `WindowedMode`, or `FullscreenMode` `ConfigEntry`.

Observation: The exact key name is not `FullscreenMode` in the binary strings that were checked.
Evidence: `strings -a ".../Baldur's Gate 3.app/Contents/MacOS/Baldur's Gate 3" | rg -i "FullscreenMode|Fullscreen|WindowedMode|ScreenWidth|ScreenHeight"` found `WindowedMode`, `Fullscreen`, `FakeFullscreenEnabled`, `ScreenHeight`, and `ScreenWidth`, but not `FullscreenMode`.

Observation: Larian-engine graphics files can omit dynamic settings, so missing entries should be inserted when headless mode needs them.
Evidence: Public BG3 user reports say the config file is dynamic and some display settings may be missing. Larian support material also distributes replacement profile folders whose `graphicSettings.lsx` is set to windowed mode as a startup recovery technique. The relevant public pages are `https://larian.com/support/faqs/performance-issues-mac_93`, `https://larian.com/support/faqs/graphical-issues_83`, and `https://steamcommunity.com/app/1086940/discussions/0/3880470363414203231/?ctp=2`.

Observation: The current background monitor already tries to hide during boot, which is the right shape for `--headless --background`, but it is fighting fullscreen Spaces today.
Evidence: `tools/bg3se_harness/_monitor.py:92-109` starts a daemon thread that calls `hide_window()` every three seconds until the socket becomes ready. This should become more reliable once BG3 starts in a normal window.

Observation: The existing `_upsert_graphic_settings()` helper writes only integer values and does not know how to restore missing entries.
Evidence: `tools/bg3se_harness/launch.py:111-171` accepts `dict[str, int]`, writes `Type` as `int32` value `0`, writes `Value` as `int32`, and never deletes a `ConfigEntry` that was inserted temporarily.

Observation: The worktree already contains many modified and untracked files unrelated to this plan.
Evidence: `git status --short` showed modified files such as `CMakeLists.txt`, `tools/bg3se_harness/launch.py`, `tools/bg3se_harness/cli.py`, `tools/bg3se_harness/config.py`, and untracked files such as `tools/bg3se_harness/_monitor.py` and earlier plan documents. Implementation must avoid broad resets.

## Decision Log

Decision: Force normal windowed mode for headless launches by writing `Fullscreen=0`, `FakeFullscreenEnabled=0`, `FakeFullscreen=0`, `ScreenWidth=1280`, and `ScreenHeight=720` before BG3 starts.
Rationale: Larian support and user reports point at `Fullscreen` and fake-fullscreen flags as the settings that control display mode. Setting both true fullscreen and fake fullscreen to false asks for a normal window. The small 1280x720 resolution reduces visual disruption if the window appears briefly. The key `FullscreenMode` was not found in checked binary strings, so it should not be the primary implementation target.
Date/Author: 2026-05-13 / Planner Agent

Decision: Treat `WindowedMode` as a live-probe fallback, not the first write.
Rationale: The executable contains the string `WindowedMode`, but the enum mapping is not established from the inspected files. Writing an unknown enum value could accidentally select fullscreen or fake fullscreen. If the primary `Fullscreen` and fake-fullscreen settings do not work in live validation, the implementer should add a separate probing milestone for `WindowedMode` values rather than mixing it into the first implementation.
Date/Author: 2026-05-13 / Planner Agent

Decision: Snapshot the original values, including missing keys, before writing temporary headless settings.
Rationale: The user should not lose their preferred fullscreen, fake fullscreen, or resolution settings. Missing keys matter because restoring should remove a temporary key that did not exist before headless launch.
Date/Author: 2026-05-13 / Planner Agent

Decision: Restore the graphics file after BG3 has been hidden, while BG3 is still running.
Rationale: BG3 needs the temporary values only at launch. Once it has created a normal window and the harness has hidden that window, the settings file can return to the user's normal configuration so the next manual launch is not affected.
Date/Author: 2026-05-13 / Planner Agent

Decision: Keep repeated hide attempts in the background monitor, but make the first attempt faster after windowed preparation.
Rationale: The problem statement says BG3 re-shows itself during boot as Bink videos and Metal windows are created. Repeated hides are appropriate for that behavior. Once BG3 is windowed, `System Events` `visible=false` should act on a normal app window instead of a fullscreen Space.
Date/Author: 2026-05-13 / Planner Agent

Decision: Minimize windows before hiding as a belt-and-suspenders behavior.
Rationale: Minimizing is not the primary fix, but it is harmless when Accessibility permissions allow it and may reduce visible flashes if BG3 creates a new normal window. The primary success condition remains `visible=false`; minimize failures should be reported but should not fail launch.
Date/Author: 2026-05-13 / Planner Agent

Decision: `show_window()` should restore the persisted graphics file before making BG3 visible.
Rationale: Showing the app is the natural manual recovery path. Restoring persisted settings there protects the next manual launch. It does not guarantee the currently running BG3 process will switch its in-memory display mode back to native fullscreen, so any current-process fullscreen toggle must be explicit and separately tested.
Date/Author: 2026-05-13 / Planner Agent

## Outcomes & Retrospective

Offline implementation is complete. The harness now snapshots the target display-mode `ConfigEntry` nodes, writes `Fullscreen=0`, `FakeFullscreenEnabled=0`, `FakeFullscreen=0`, `ScreenWidth=1280`, and `ScreenHeight=720` only for headless launches, and restores the prior values or missing-key state after hide/failure paths. Foreground launch, `test --headless`, `show_window()`, and the background monitor all call restore.

Offline validation passed:

```bash
PYTHONPATH=tools pytest tests/harness/test_headless_graphics.py -v
PYTHONPATH=tools pytest tests/harness/ -v
PYTHONPATH=tools python3 -m bg3se_harness launch --help
PYTHONPATH=tools python3 -m bg3se_harness test --help
```

Live validation is still pending, so this document does not yet record whether the primary `Fullscreen=0` and fake-fullscreen-off settings are sufficient in a real BG3 boot or whether `WindowedMode` probing is needed.

## Context and Orientation

The Python harness lives in `tools/bg3se_harness/`. The command `PYTHONPATH=tools python3 -m bg3se_harness launch` builds and deploys the Script Extender dylib, patches the BG3 executable, launches BG3, and waits for the Script Extender Unix socket at `/tmp/bg3se.sock`. A Unix socket is a local file-like communication endpoint used here to send Lua commands into the running game.

The command-line entry point is `tools/bg3se_harness/cli.py`. The function `cmd_launch()` starts at `tools/bg3se_harness/cli.py:151`. It reads `--headless`, `--background`, and `--timeout`. In background mode it starts `tools/bg3se_harness/_monitor.py` as a detached subprocess and returns JSON containing the BG3 PID plus `/tmp/bg3se_health.json` and `/tmp/bg3se_monitor.log`.

The launch logic is in `tools/bg3se_harness/launch.py`. The function `launch()` starts at `tools/bg3se_harness/launch.py:174`. It kills old harness-owned BG3 processes, removes stale sockets, writes launcher and video-skip preferences, builds the command `arch -arm64 <BG3 executable>`, sets environment variables, and calls `subprocess.Popen`. It currently does not know whether the launch is headless.

The graphics-file helper is `_upsert_graphic_settings()` in `tools/bg3se_harness/launch.py:111-171`. It parses the XML file at `GRAPHIC_SETTINGS_PATH`, finds the `<children>` node containing `ConfigEntry` nodes, then inserts or updates integer entries. It currently creates `graphicSettings.lsx.bak` only once, but that backup is not enough for per-launch restore because it does not record which keys the harness temporarily inserted.

The path constants are in `tools/bg3se_harness/config.py`. `GRAPHIC_SETTINGS_PATH` is `Path.home() / "Documents/Larian Studios/Baldur's Gate 3" / "graphicSettings.lsx"`. `HARNESS_CONFIG_DIR` is `Path.home() / ".config/bg3se-harness"`. The restore metadata for this plan should live under `HARNESS_CONFIG_DIR`, not inside the BG3 app bundle.

The background monitor is `tools/bg3se_harness/_monitor.py`. It creates a fake process object from the launched PID, starts a persistent hide thread when `headless` is true, polls the socket with `wait_for_socket()`, writes health JSON, and hides again after socket readiness. This is where `--headless --background` should restore the user's graphics file after a successful hide or after a failed boot.

The existing hide and show functions are in `tools/bg3se_harness/launch.py:361-398`. `hide_window()` runs AppleScript through `osascript` to tell System Events to set the BG3 process `visible` property to false, then activates a terminal app. `show_window()` sets the same process visible again. A `visible` property is macOS Accessibility state for whether an application process is hidden.

The relevant graphics settings are `ConfigEntry` nodes in `graphicSettings.lsx`. A `ConfigEntry` has a `MapKey` attribute containing the setting name, a `Type` attribute, and a `Value` attribute. For integer settings in the currently generated file, `Type` has `value="0"` and `Value` has `type="int32"`. The headless display-mode entries should follow that existing integer shape.

## Plan of Work

First, add graphics snapshot and restore helpers in `tools/bg3se_harness/launch.py`. The implementation should import `HARNESS_CONFIG_DIR` from `tools/bg3se_harness/config.py` and define a restore metadata path such as `HEADLESS_GRAPHICS_RESTORE_PATH = HARNESS_CONFIG_DIR / "graphic_settings_headless_restore.json"`. Add a constant for the headless values: `Fullscreen=0`, `FakeFullscreenEnabled=0`, `FakeFullscreen=0`, `ScreenWidth=1280`, and `ScreenHeight=720`. The helper must record whether each original key existed. If it existed, record the full XML attribute state needed to put it back. If it did not exist, record that it was missing so restore can delete the temporary node.

Second, update or replace `_upsert_graphic_settings()` so it can support both temporary insertion and exact restoration. The existing helper can remain for `SkipVideo` and `SkipSplashScreen`, but the new code needs lower-level XML operations: find entries by `MapKey`, create integer entries, update existing integer entries, remove entries that were originally missing, and write the XML back. Keep writes best-effort and diagnostic. Do not make a graphics-file failure crash launch unless the XML is so broken that headless mode cannot be attempted and the caller explicitly requested strict behavior.

Third, change `launch()` in `tools/bg3se_harness/launch.py` to accept `headless=False`. The call to prepare headless graphics must happen after `ensure_no_launcher()` and before `subprocess.Popen`. It should happen only when `headless` is true. It should return a structured dictionary that can be logged or attached to health JSON, but `launch()` should continue returning the `Popen` object to preserve current callers. A simple way to preserve that interface is to store the structured result on the process object as `proc.bg3se_headless_graphics = result` immediately after `Popen`.

Fourth, update `tools/bg3se_harness/cli.py` so `cmd_launch()` passes `headless=headless` into `launch_mod.launch(...)`. For non-background `launch --headless`, keep the existing behavior of waiting for the socket and then hiding. Immediately after the hide attempt, call `launch_mod.restore_headless_graphics(reason="foreground_hide_complete")` and include both hide and restore results in the JSON under `headless`. If socket readiness fails or BG3 exits early, restore before returning failure so the user's next manual launch is not left windowed.

Fifth, update `cmd_test()` in `tools/bg3se_harness/cli.py` the same way if test mode supports `--headless`. It should pass `headless=headless` into `launch_mod.launch(...)`, hide after socket readiness as it does now, restore after the hide attempt, and include the restore result in the launch JSON. Test mode should not leave the persisted graphics file in headless mode while the tests run.

Sixth, update `tools/bg3se_harness/_monitor.py` for background mode. The persistent hide loop should start with a shorter first delay, such as one second, because the process has already been configured to create a window instead of a fullscreen Space. It should keep retrying because BG3 can re-show itself during videos and Metal window creation. When the monitor performs the final hide after socket readiness, it should call `restore_headless_graphics(reason="background_hide_complete")` and record the restore result in `health["headless"]["graphics_restore"]`. If the socket times out or the process exits, the monitor should still call restore with a failure reason before writing final health JSON.

Seventh, strengthen `hide_window()` in `tools/bg3se_harness/launch.py`. Before setting `visible` to false, it should best-effort minimize BG3 windows through System Events Accessibility, for example by setting the `AXMinimized` attribute on each window when available. The return dictionary should include `success`, `method`, `minimized`, and any non-fatal minimize error. Hiding should be considered successful only if the `visible=false` AppleScript succeeds. Minimize failure should not fail headless mode.

Eighth, update `show_window()` in `tools/bg3se_harness/launch.py`. It should call `restore_headless_graphics(reason="show_window")` before setting BG3 visible and activating it. Its return dictionary should include both `visible` state and `graphics_restore` state. Do not silently send a native fullscreen toggle by default, because doing so can create a fullscreen Space again. If implementers add a current-process fullscreen option later, it should be an explicit parameter such as `show_window(toggle_native_fullscreen=False)` and should have separate live validation.

Ninth, add offline tests. The tests should use a temporary XML file and monkeypatch `launch.GRAPHIC_SETTINGS_PATH` and `launch.HEADLESS_GRAPHICS_RESTORE_PATH` so they do not touch the real BG3 profile. Tests should prove that missing display-mode keys are inserted for headless launch, existing keys are restored exactly, missing keys are removed after restore, restore is safe when no restore file exists, `cmd_launch()` passes `headless=True` to `launch()`, and `_monitor.py` restores after both successful and failed background runs.

Tenth, update documentation in `docs/harness.md`. The docs should say that macOS headless mode means "temporary windowed launch plus app hide", not "true game-engine headless rendering." Explain that fullscreen BG3 cannot be reliably hidden because macOS gives it a fullscreen Space. Explain that the harness restores `graphicSettings.lsx` after hiding so normal manual launches keep the user's previous display mode.

## Concrete Steps

Run all commands from `/Users/tomdimino/Desktop/Programming/game-modding/bg3/bg3se-macos`.

Step 1: Re-read the current code before editing.

```bash
nl -ba tools/bg3se_harness/launch.py | sed -n '80,220p'
nl -ba tools/bg3se_harness/launch.py | sed -n '300,405p'
nl -ba tools/bg3se_harness/_monitor.py | sed -n '80,175p'
nl -ba tools/bg3se_harness/cli.py | sed -n '151,245p'
```

Expected output: The first command shows `ensure_skip_videos()`, `_upsert_graphic_settings()`, and `launch()`. The second command shows `wait_for_socket()`, `hide_window()`, and `show_window()`. The third command shows the persistent hide loop and final hide block in the monitor. The fourth command shows `cmd_launch()` passing no headless parameter into `launch_mod.launch(...)`.

Step 2: Add restore-path imports and constants in `tools/bg3se_harness/launch.py`.

Add `HARNESS_CONFIG_DIR` to the existing import from `.config`. Define these module constants near the existing launch helpers:

```python
HEADLESS_GRAPHICS_RESTORE_PATH = HARNESS_CONFIG_DIR / "graphic_settings_headless_restore.json"
HEADLESS_GRAPHICS_ENTRIES = {
    "Fullscreen": 0,
    "FakeFullscreenEnabled": 0,
    "FakeFullscreen": 0,
    "ScreenWidth": 1280,
    "ScreenHeight": 720,
}
```

Expected result: importing `bg3se_harness.launch` still works, and existing video-skip code is unchanged.

Step 3: Add XML helpers in `tools/bg3se_harness/launch.py`.

Implement helpers with names close to these:

```python
def _load_graphic_settings_tree():
    """Return (tree, config_children) for graphicSettings.lsx, or (None, None)."""

def _index_graphic_settings(config_children):
    """Return a dict mapping MapKey value to ConfigEntry element."""

def _snapshot_graphic_settings(keys):
    """Return JSON-serializable original state for keys."""

def _write_int_graphic_settings(entries):
    """Insert or update integer ConfigEntry values."""

def _restore_graphic_settings_snapshot(snapshot):
    """Restore existing entries and remove entries that were originally missing."""
```

The snapshot must preserve each existing entry's `MapKey` type, `Type` type and value, and `Value` type and value. The restore function must delete a temporary node if the snapshot says `existed` was false. All functions should use `xml.etree.ElementTree`, matching the current file.

Expected result: `_upsert_graphic_settings({"SkipVideo": 1})` still works, and the new helpers can be called by tests without launching BG3.

Step 4: Add public headless graphics functions in `tools/bg3se_harness/launch.py`.

Implement:

```python
def prepare_headless_graphics():
    """Temporarily force BG3 graphics settings to normal windowed mode."""

def restore_headless_graphics(reason=""):
    """Restore graphics settings saved by prepare_headless_graphics()."""
```

`prepare_headless_graphics()` should create `HARNESS_CONFIG_DIR`, create a restore file only if one does not already exist, then write `HEADLESS_GRAPHICS_ENTRIES`. If a restore file already exists, do not overwrite it. That protects the original user settings across repeated launches or monitor crashes. Return a dictionary with `success`, `path`, `restore_path`, `entries`, and whether a new snapshot was created.

`restore_headless_graphics()` should return success when there is no restore file because there is nothing to restore. When a restore file exists, it should apply the snapshot, delete the restore file only after the XML write succeeds, and return a dictionary with `success`, `restored`, `removed`, and `reason`.

Expected result: these functions can be run repeatedly in tests. Running restore twice should not fail.

Step 5: Wire headless preparation into `launch()`.

Change the signature from:

```python
def launch(continue_game=False, load_save=None, extra_flags=None,
           skip_videos=True, auto_dismiss=True):
```

to:

```python
def launch(continue_game=False, load_save=None, extra_flags=None,
           skip_videos=True, auto_dismiss=True, headless=False):
```

Inside `launch()`, call `prepare_headless_graphics()` only when `headless` is true and before building or starting the BG3 process. After `Popen`, attach the result to the process object:

```python
proc.bg3se_headless_graphics = headless_graphics
```

Expected result: existing callers that do not pass `headless` behave the same. Headless callers force the XML values before BG3 starts.

Step 6: Pass `headless` from `tools/bg3se_harness/cli.py`.

In `cmd_launch()`, move `headless = getattr(args, "headless", False)` before the call to `launch_mod.launch(...)`, then pass `headless=headless`. In non-background mode, after `hide_window()` or after any socket failure, call `restore_headless_graphics()`. Include the restore result in JSON.

In `cmd_test()`, make the same launch call change and restore after the hide attempt. If socket readiness fails, restore before returning `1`.

Expected result: unit tests can assert `launch()` receives `headless=True` when the CLI argument is set.

Step 7: Update the background monitor in `tools/bg3se_harness/_monitor.py`.

Import `restore_headless_graphics` from `bg3se_harness.launch`. Change the persistent hide thread's first delay from three seconds to about one second, then continue retrying at a short interval such as one or two seconds until socket readiness or until the attempt budget expires. After the final hide block, call restore and record it. If there is no successful socket connection, still call restore before writing final health.

Expected monitor log for a good run: a line showing early hide, a line showing socket readiness, a line showing final hide, and a line showing graphics restore.

Step 8: Add minimize support to `hide_window()`.

Replace the single AppleScript command with a script that first tries to minimize windows and then hides the process. One acceptable shape is:

```applescript
tell application "System Events"
  if exists process "Baldur's Gate 3" then
    tell process "Baldur's Gate 3"
      repeat with w in windows
        try
          set value of attribute "AXMinimized" of w to true
        end try
      end repeat
      set visible to false
    end tell
  end if
end tell
```

The Python return value should distinguish hide success from minimize success. If Accessibility permissions reject the minimize attribute but allow hiding, return `success=True` with a non-fatal minimize diagnostic.

Expected result: `hide_window()` remains safe to call repeatedly during boot.

Step 9: Update `show_window()`.

Call `restore_headless_graphics(reason="show_window")` before making the BG3 process visible. Then run System Events `visible=true` and activate BG3. Return one dictionary containing `success`, `method`, and `graphics_restore`.

Expected result: a developer can run `PYTHONPATH=tools python3 -c 'from bg3se_harness.launch import show_window; print(show_window())'` to recover a hidden BG3 window and restore persisted graphics settings.

Step 10: Add or update offline tests.

Create or extend tests under `tests/harness/`. A suitable new file is `tests/harness/test_headless_graphics.py`. Use temporary XML with the same `ConfigEntry` shape as BG3. Test at least these cases: all headless keys missing before prepare, `Fullscreen` and resolution keys existing before prepare, restore after prepare removes newly inserted keys, restore after prepare restores original values, second prepare does not overwrite an existing restore snapshot, and restore without a restore file is a no-op success.

Extend `tests/harness/test_cli.py` so `test_cmd_launch_headless_hides_after_socket` also asserts the fake `launch()` saw `headless=True`. Add a background monitor test if the repository already has a monitor-test pattern; otherwise keep monitor behavior validated through live testing and record that gap in Outcomes.

Expected output from test runs: all selected pytest tests pass.

Step 11: Update docs.

Edit `docs/harness.md` near the launch or troubleshooting section. Add a short explanation that macOS headless mode temporarily writes windowed graphics settings, starts BG3, hides the normal app window, then restores `graphicSettings.lsx` for future manual launches. Define that this is not a renderer-level headless mode; BG3 still runs, but the app is hidden.

Expected result: a reader understands why a brief window flash can still happen and why the fullscreen setting is not permanently changed.

Step 12: Run offline validation.

```bash
PYTHONPATH=tools pytest tests/harness/test_headless_graphics.py -q
PYTHONPATH=tools pytest tests/harness/test_launch.py tests/harness/test_cli.py -q
PYTHONPATH=tools python3 -m bg3se_harness launch --help
PYTHONPATH=tools python3 -m bg3se_harness test --help
```

Expected output: pytest exits 0. The help commands exit 0 and still show `--headless`, `--background`, and `--no-skip-videos`.

Step 13: Run live background validation.

```bash
PYTHONPATH=tools python3 -m bg3se_harness quit --force
PYTHONPATH=tools python3 -m bg3se_harness launch --headless --background --timeout 90
sleep 12
osascript -e 'tell application "System Events" to if exists process "Baldur'\''s Gate 3" then get visible of process "Baldur'\''s Gate 3"'
tail -80 /tmp/bg3se_monitor.log
PYTHONPATH=tools python3 -m bg3se_harness status
```

Expected output: the launch command returns JSON with `"background": true`. The `osascript` command prints `false` after the hide attempts begin. The monitor log shows hide attempts during boot and either `socket_ready` or a clear failure reason. The status command eventually includes health JSON with `"socket_connected": true` for a successful run.

Step 14: Confirm graphics restoration after live background validation.

```bash
python3 - <<'PY'
from pathlib import Path
import xml.etree.ElementTree as ET

path = Path.home() / "Documents/Larian Studios/Baldur's Gate 3" / "graphicSettings.lsx"
tree = ET.parse(path)
values = {}
for node in tree.getroot().iter("node"):
    if node.get("id") != "ConfigEntry":
        continue
    key = None
    value = None
    for attr in node.findall("attribute"):
        if attr.get("id") == "MapKey":
            key = attr.get("value")
        elif attr.get("id") == "Value":
            value = attr.get("value")
    if key in {"Fullscreen", "FakeFullscreenEnabled", "FakeFullscreen", "ScreenWidth", "ScreenHeight"}:
        values[key] = value
print(values)
PY
test ! -f "$HOME/.config/bg3se-harness/graphic_settings_headless_restore.json"
```

Expected output: the printed values match the user's pre-launch snapshot rather than always showing the temporary `1280x720` and fullscreen-off values. The `test ! -f ...` command exits 0 after restore.

Step 15: Run live foreground validation.

```bash
PYTHONPATH=tools python3 -m bg3se_harness quit --force
PYTHONPATH=tools python3 -m bg3se_harness launch --headless --timeout 90
```

Expected output: the command waits for socket readiness, hides BG3, restores graphics settings, and prints JSON containing `"socket_connected": true` and a `"headless"` object with both hide and graphics restore information.

## Validation and Acceptance

Acceptance criterion 1: `launch()` writes temporary windowed graphics settings before `subprocess.Popen` starts BG3, and only when `headless=True`.

Acceptance criterion 2: The temporary settings include `Fullscreen=0`, `FakeFullscreenEnabled=0`, `FakeFullscreen=0`, `ScreenWidth=1280`, and `ScreenHeight=720` as integer `ConfigEntry` values. `FullscreenMode` is not required because it was not found in the checked binary strings.

Acceptance criterion 3: The original values and missing-key state are restored after the hide phase. If a key did not exist before launch, it is removed during restore. If a key existed, its original value and XML attribute types are restored.

Acceptance criterion 4: `launch --headless --background` returns immediately and the monitor hides BG3 during boot without leaving the user in a BG3 fullscreen Space. The monitor health JSON records hide and restore results.

Acceptance criterion 5: BG3 can still reach the Script Extender socket in headless background mode. Success means `/tmp/bg3se_health.json` or `bg3se-harness status` reports `"socket_connected": true`.

Acceptance criterion 6: `hide_window()` reports minimize and hide diagnostics separately. A minimize failure does not fail headless mode if `visible=false` succeeds.

Acceptance criterion 7: `show_window()` restores persisted graphics settings before making BG3 visible. The current running process may remain windowed unless an explicit, separately tested fullscreen toggle is added.

Acceptance criterion 8: If BG3 exits early, the socket times out, or the monitor fails, the harness attempts graphics restore before returning or writing final health JSON.

Acceptance criterion 9: Offline tests exercise XML insert, update, missing-key restore, existing-key restore, idempotent restore, and CLI propagation of `headless=True`.

## Idempotence and Recovery

The plan is idempotent because `prepare_headless_graphics()` should not overwrite an existing restore snapshot, and `restore_headless_graphics()` should be a no-op success when no restore file exists. Repeating `launch --headless --background` should keep the first original snapshot until restore succeeds, then create a fresh snapshot on the next run.

If a live run fails and leaves BG3 running, recover with:

```bash
PYTHONPATH=tools python3 -m bg3se_harness quit --force
PYTHONPATH=tools python3 -c 'from bg3se_harness.launch import restore_headless_graphics; print(restore_headless_graphics(reason="manual_recovery"))'
```

If BG3 is hidden and the developer wants to inspect it, recover with:

```bash
PYTHONPATH=tools python3 -c 'from bg3se_harness.launch import show_window; print(show_window())'
```

If the restore JSON exists but the helper cannot restore it, inspect it directly:

```bash
cat "$HOME/.config/bg3se-harness/graphic_settings_headless_restore.json"
```

If the XML file becomes corrupted during development, restore from the existing Larian profile backup if present:

```bash
cp "$HOME/Documents/Larian Studios/Baldur's Gate 3/graphicSettings.lsx.bak" "$HOME/Documents/Larian Studios/Baldur's Gate 3/graphicSettings.lsx"
```

Do not run `git reset --hard` to recover this worktree. The repository already has unrelated modified and untracked files. Revert only files touched by this plan if needed: `tools/bg3se_harness/launch.py`, `tools/bg3se_harness/cli.py`, `tools/bg3se_harness/_monitor.py`, `tests/harness/test_headless_graphics.py`, `tests/harness/test_cli.py`, `docs/harness.md`, and this plan file.

## Interfaces and Dependencies

`tools/bg3se_harness/launch.py` must provide:

```python
HEADLESS_GRAPHICS_ENTRIES: dict[str, int]
HEADLESS_GRAPHICS_RESTORE_PATH: Path

def prepare_headless_graphics() -> dict:
    """Temporarily force BG3 graphics settings to normal windowed mode."""

def restore_headless_graphics(reason: str = "") -> dict:
    """Restore graphics settings saved by prepare_headless_graphics()."""

def launch(continue_game=False, load_save=None, extra_flags=None,
           skip_videos=True, auto_dismiss=True, headless=False):
    """Launch BG3 and optionally prepare temporary headless graphics first."""

def hide_window() -> dict:
    """Minimize BG3 windows best-effort, then hide the BG3 app process."""

def show_window() -> dict:
    """Restore persisted graphics settings, then show and activate BG3."""
```

The returned dictionaries should all include `success: bool`. Graphics functions should include `path` and `restore_path`. Hide and show functions should include a `method` string and any diagnostic error text.

`tools/bg3se_harness/cli.py` must pass `headless=True` into `launch_mod.launch(...)` for `launch --headless` and `test --headless`.

`tools/bg3se_harness/_monitor.py` must call `restore_headless_graphics(...)` before final health JSON is written whenever `headless` is true.

No new third-party dependency is required. The implementation uses Python standard-library modules already present in the file: `json`, `shutil`, `subprocess`, `time`, and `xml.etree.ElementTree`. It also uses the existing macOS command-line tool `osascript` and System Events Accessibility permissions already required by `hide_window()`.
