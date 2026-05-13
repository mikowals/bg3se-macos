"""Background health monitor for headless launch.

Spawned as a detached subprocess by `launch --background`. Polls the
SE socket, writes health JSON to HEALTH_FILE, and optionally hides
the BG3 window.

Writes a human-readable stage log to MONITOR_LOG that can be tailed
to watch the boot sequence in real time:
    tail -f /tmp/bg3se_monitor.log

Usage (internal — called by cli.py):
    python3 -m bg3se_harness._monitor <pid> <timeout> <headless:0|1>
"""

import json
import os
import sys
import time

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from bg3se_harness.config import HEALTH_FILE, MONITOR_LOG
from bg3se_harness.launch import wait_for_socket, hide_window, restore_headless_graphics


def _log(msg, stages):
    """Write a timestamped line to the monitor log and track in stages list."""
    ts = time.strftime("%H:%M:%S")
    elapsed = time.monotonic() - _start_time
    line = f"[{ts}] +{elapsed:6.1f}s  {msg}"
    stages.append({"t": round(elapsed, 1), "msg": msg})
    try:
        with open(MONITOR_LOG, "a") as f:
            f.write(line + "\n")
    except OSError:
        pass

_start_time = 0.0


class _FakeProcess:
    """Minimal stand-in for subprocess.Popen — only needs poll() and pid."""

    def __init__(self, pid):
        self.pid = pid
        self.returncode = None

    def poll(self):
        try:
            os.kill(self.pid, 0)
            return None
        except ProcessLookupError:
            self.returncode = -1
            return -1
        except PermissionError:
            return None


def main():
    global _start_time
    if len(sys.argv) < 4:
        print("Usage: _monitor.py <pid> <timeout> <headless:0|1> [skip_videos:0|1] [auto_dismiss:0|1]",
              file=sys.stderr)
        sys.exit(1)

    pid = int(sys.argv[1])
    timeout = int(sys.argv[2])
    headless = sys.argv[3] == "1"
    skip_videos = sys.argv[4] == "1" if len(sys.argv) > 4 else False
    auto_dismiss = sys.argv[5] == "1" if len(sys.argv) > 5 else False

    _start_time = time.monotonic()
    stages = []

    # Clear previous log, write header
    try:
        with open(MONITOR_LOG, "w") as f:
            f.write(f"=== BG3SE Monitor (pid {pid}, timeout {timeout}s, headless={headless}) ===\n")
    except OSError:
        pass

    proc = _FakeProcess(pid)

    _log(f"monitor_started  pid={pid} timeout={timeout}s headless={headless}", stages)

    HEALTH_FILE.write_text(json.dumps({
        "status": "monitoring",
        "pid": pid,
        "started_at": time.time(),
    }))

    # No persistent hide during boot — hiding too early prevents Metal
    # from creating a drawable, stalling the game. Hide only after socket
    # connects (see final hide block below).

    # Check if BG3SE dylib log appears (indicates injection worked)
    log_dir = os.path.expanduser("~/Library/Application Support/BG3SE/logs")
    latest_log = os.path.join(log_dir, "latest.log")
    if os.path.exists(latest_log):
        mtime = os.path.getmtime(latest_log)
        started_at = time.time()
        try:
            started_at = json.loads(HEALTH_FILE.read_text()).get("started_at", started_at)
        except (FileNotFoundError, json.JSONDecodeError):
            pass
        if mtime > started_at - 5:
            _log("dylib_detected   BG3SE log file present (fresh)", stages)
        else:
            _log("dylib_stale      BG3SE log from previous session", stages)
    else:
        _log("dylib_pending    waiting for BG3SE log file...", stages)

    if skip_videos:
        _log("video_skip       BG3SE_SKIP_VIDEOS=1 (in-process Bink hook)", stages)

    if auto_dismiss:
        _log("auto_dismiss     BG3SE_AUTO_DISMISS_SPLASH=1 (in-process dismiss)", stages)

    _log("socket_polling   waiting for SE socket to respond...", stages)

    health = wait_for_socket(
        timeout=timeout, dismiss_splash=True, process=proc,
    )
    health["pid"] = pid

    if health.get("socket_connected"):
        elapsed_ms = health.get("elapsed_ms", 0)
        _log(f"socket_ready     connected in {elapsed_ms}ms", stages)
    elif health.get("stage") == "process_exited":
        code = health.get("exitcode", "?")
        _log(f"process_exited   BG3 died during boot (exit code {code})", stages)
    else:
        elapsed_ms = health.get("elapsed_ms", 0)
        _log(f"socket_timeout   no response after {elapsed_ms}ms", stages)

    if headless:
        if health.get("socket_connected"):
            _log("hiding_window    requesting System Events hide...", stages)
            hr = hide_window()
            health["headless"] = {
                "requested": True,
                "hidden": hr.get("success", False),
                **hr,
            }
            if hr.get("success"):
                _log("window_hidden    BG3 running in background", stages)
            else:
                _log(f"hide_failed      {hr.get('error', 'unknown')}", stages)
            restore_reason = "background_hide_complete"
        else:
            health["headless"] = {
                "requested": True,
                "hidden": False,
                "reason": health.get("stage", "socket_not_connected"),
            }
            restore_reason = "background_socket_not_connected"

        restore_result = restore_headless_graphics(reason=restore_reason)
        health["headless"]["graphics_restore"] = restore_result
        if restore_result.get("success"):
            _log("graphics_restore restored headless launch settings", stages)
        else:
            _log(f"restore_failed   {restore_result.get('error', 'unknown')}", stages)

    health["boot_stages"] = stages
    HEALTH_FILE.write_text(json.dumps(health, indent=2))

    status = "ready" if health.get("socket_connected") else "failed"
    _log(f"monitor_done     status={status}", stages)

    # Final flush to log
    try:
        with open(MONITOR_LOG, "a") as f:
            f.write(f"=== Monitor complete ({status}) ===\n")
    except OSError:
        pass


if __name__ == "__main__":
    main()
