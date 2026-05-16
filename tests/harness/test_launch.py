"""Tests for bg3se_harness.launch — wait_for_socket lifecycle."""

from types import SimpleNamespace

import pytest

from bg3se_harness import launch


def _make_clock(step=0.6):
    """Return a monotonic mock that advances `step` seconds per call."""
    state = [0.0]
    def tick():
        val = state[0]
        state[0] += step
        return val
    return tick


def test_wait_for_socket_reports_process_exit():
    proc = SimpleNamespace(returncode=42)
    proc.poll = lambda: 42
    result = launch.wait_for_socket(timeout=2, process=proc)
    assert result["socket_connected"] is False
    assert result["stage"] == "process_exited"
    assert result["exitcode"] == 42


def test_wait_for_socket_returns_false_on_timeout(monkeypatch):
    monkeypatch.setattr(launch.time, "monotonic", _make_clock(step=0.6))
    monkeypatch.setattr(launch.time, "sleep", lambda _: None)

    class NoSocket:
        def settimeout(self, _): pass
        def connect(self, _): raise ConnectionRefusedError()
        def close(self): pass

    monkeypatch.setattr(launch.socket, "socket", lambda *a, **k: NoSocket())

    result = launch.wait_for_socket(timeout=1)
    assert result["socket_connected"] is False
    assert result["stage"] == "timeout"


def test_wait_for_socket_ocr_called_after_delay(monkeypatch):
    """With dismiss_splash=True, OCR menu detection runs after dismiss_delay."""
    calls = []
    monkeypatch.setattr(launch.time, "monotonic", _make_clock(step=2.0))
    monkeypatch.setattr(launch.time, "sleep", lambda _: None)
    monkeypatch.setattr(
        launch, "_detect_main_menu",
        lambda: (calls.append("ocr"), (False, []))[1],
    )

    class NoSocket:
        def settimeout(self, _): pass
        def connect(self, _): raise ConnectionRefusedError()
        def close(self): pass

    monkeypatch.setattr(launch.socket, "socket", lambda *a, **k: NoSocket())

    result = launch.wait_for_socket(timeout=20, dismiss_splash=True)
    assert result["socket_connected"] is False
    assert "ocr" in calls


def test_wait_for_socket_no_dismiss_when_disabled(monkeypatch):
    calls = []
    monkeypatch.setattr(launch.time, "monotonic", _make_clock(step=3.0))
    monkeypatch.setattr(launch.time, "sleep", lambda _: None)
    monkeypatch.setattr(
        launch, "_try_dismiss_splash",
        lambda n, pid=None: calls.append(("dismiss", n)),
    )

    class NoSocket:
        def settimeout(self, _): pass
        def connect(self, _): raise ConnectionRefusedError()
        def close(self): pass

    monkeypatch.setattr(launch.socket, "socket", lambda *a, **k: NoSocket())

    launch.wait_for_socket(timeout=10, dismiss_splash=False)
    assert calls == []


def test_wait_for_socket_menu_stall_runs_watchdog_and_aborts(monkeypatch):
    calls = []
    monkeypatch.setattr(launch.time, "monotonic", _make_clock(step=8.0))
    monkeypatch.setattr(launch.time, "sleep", lambda _: None)
    monkeypatch.setattr(launch, "_detect_main_menu", lambda: (False, []))
    monkeypatch.setattr(
        launch, "_try_watchdog_continue",
        lambda pid, attempt: calls.append((pid, attempt)) or {
            "attempt": attempt,
            "method": "test_watchdog",
            "success": True,
        },
    )
    monkeypatch.setattr(
        launch, "_collect_boot_diagnostics",
        lambda: {"latest_log": "latest.log"},
    )

    class QuietSocket:
        def settimeout(self, _): pass
        def connect(self, _): pass
        def recv(self, _): raise launch.socket.timeout()
        def sendall(self, _): pass
        def close(self): pass

    proc = SimpleNamespace(pid=99, returncode=None, poll=lambda: None)
    monkeypatch.setattr(launch.socket, "socket", lambda *a, **k: QuietSocket())

    result = launch.wait_for_socket(timeout=200, dismiss_splash=True, process=proc)

    assert result["socket_connected"] is False
    assert result["stage"] == "menu_stalled"
    assert len(calls) == launch._MENU_STALL_MAX_ACTIONS
    assert result["diagnostics"] == {"latest_log": "latest.log"}


def test_watchdog_sequence_includes_mod_verification_start_game():
    purposes = [
        launch._watchdog_target_for_attempt(attempt)[0]
        for attempt in range(1, launch._MENU_STALL_MAX_ACTIONS + 1)
    ]

    assert purposes == [
        "dismiss_splash",
        "click_continue",
        "check_mod_verification_boxes",
        "click_mod_verification_start_game",
        "check_mod_verification_boxes",
        "click_mod_verification_start_game",
    ]
