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


def test_wait_for_socket_dismiss_called_after_delay(monkeypatch):
    calls = []
    monkeypatch.setattr(launch.time, "monotonic", _make_clock(step=2.0))
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

    result = launch.wait_for_socket(timeout=20, dismiss_splash=True)
    assert result["socket_connected"] is False
    assert any(c[0] == "dismiss" for c in calls)


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
