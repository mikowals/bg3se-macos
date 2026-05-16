"""Tests for bg3se_harness.cli — cmd_launch, cmd_build, cmd_test."""

import argparse
import json
import sys
from types import SimpleNamespace

import pytest

from bg3se_harness import cli


def _stub_build_deploy(monkeypatch):
    """Stub out build+verify+deploy to always succeed."""
    monkeypatch.setattr(cli.build_mod, "build", lambda **k: {"success": True})
    monkeypatch.setattr(cli.build_mod, "verify", lambda **k: {"verified": True})
    monkeypatch.setattr(cli.build_mod, "deploy", lambda **k: {"deployed": True})


def _stub_patch(monkeypatch):
    monkeypatch.setattr(cli.patch_mod, "patch", lambda **k: {"success": True})


# ── cmd_launch headless ──────────────────────────────────────────────


def test_cmd_launch_headless_hides_after_socket(monkeypatch, capsys):
    calls = []

    _stub_build_deploy(monkeypatch)
    _stub_patch(monkeypatch)
    monkeypatch.setattr(
        cli.launch_mod, "launch",
        lambda **k: (calls.append(("launch", k)) or SimpleNamespace(pid=123, poll=lambda: None)),
    )
    monkeypatch.setattr(cli.launch_mod, "default_timeout", lambda *a: 1)
    monkeypatch.setattr(
        cli.launch_mod, "wait_for_socket",
        lambda **k: (calls.append("wait") or {"socket_connected": True, "elapsed_ms": 500}),
    )
    monkeypatch.setattr(
        cli.launch_mod, "hide_window",
        lambda: (calls.append("hide") or {"success": True}),
    )
    monkeypatch.setattr(
        cli.launch_mod, "restore_headless_graphics",
        lambda **k: (calls.append(("restore", k)) or {"success": True}),
    )

    args = argparse.Namespace(
        headless=True, timeout=1, continue_game=False, save=None,
        skip_videos=True, storylog=False, stats=False, json_mode=False,
        osi_debug=False, syslog=False, modded=False, controller=False,
        ecb_checker=False, module=None, detail_level=None, log_path=None,
        flags=None,
    )
    rc = cli.cmd_launch(args)
    assert rc == 0
    assert "wait" in calls
    assert "hide" in calls
    assert calls.index("wait") < calls.index("hide")
    launch_call = next(item for item in calls if isinstance(item, tuple) and item[0] == "launch")
    assert launch_call[1]["headless"] is True
    assert any(item for item in calls if isinstance(item, tuple) and item[0] == "restore")


def test_cmd_launch_headless_does_not_hide_on_socket_failure(monkeypatch, capsys):
    _stub_build_deploy(monkeypatch)
    _stub_patch(monkeypatch)
    monkeypatch.setattr(
        cli.launch_mod, "launch",
        lambda **k: SimpleNamespace(pid=123, poll=lambda: None),
    )
    monkeypatch.setattr(cli.launch_mod, "default_timeout", lambda *a: 1)
    monkeypatch.setattr(
        cli.launch_mod, "wait_for_socket",
        lambda **k: {"socket_connected": False, "elapsed_ms": 1000},
    )
    monkeypatch.setattr(
        cli.launch_mod, "hide_window",
        lambda: pytest.fail("hide_window should not be called"),
    )
    monkeypatch.setattr(
        cli.launch_mod, "restore_headless_graphics",
        lambda **k: {"success": True},
    )

    args = argparse.Namespace(
        headless=True, timeout=1, continue_game=False, save=None,
        skip_videos=True, storylog=False, stats=False, json_mode=False,
        osi_debug=False, syslog=False, modded=False, controller=False,
        ecb_checker=False, module=None, detail_level=None, log_path=None,
        flags=None,
    )
    rc = cli.cmd_launch(args)
    assert rc == 1


def test_cmd_launch_background_returns_without_foreground_wait(monkeypatch, capsys):
    calls = []

    class FakePopen:
        def __init__(self, *a, **k):
            calls.append(("monitor", a, k))

    _stub_build_deploy(monkeypatch)
    _stub_patch(monkeypatch)
    monkeypatch.setattr(
        cli.launch_mod, "launch",
        lambda **k: calls.append(("launch", k)) or SimpleNamespace(pid=123),
    )
    monkeypatch.setattr(cli.launch_mod, "default_timeout", lambda *a: 1)
    monkeypatch.setattr(
        cli.launch_mod, "wait_for_socket",
        lambda **k: pytest.fail("background launch should not wait in foreground"),
    )
    monkeypatch.setattr(cli, "sys", SimpleNamespace(
        executable="python3",
        stderr=sys.stderr,
    ))
    monkeypatch.setattr(cli.launch_mod, "HEALTH_FILE", "/tmp/health.json", raising=False)
    monkeypatch.setattr("subprocess.Popen", FakePopen)

    args = argparse.Namespace(
        background=True, headless=True, timeout=1, continue_game=True, save=None,
        boot_retries=1, retry_delay=0,
        skip_videos=True, storylog=False, stats=False, json_mode=False,
        osi_debug=False, syslog=False, modded=False, controller=False,
        ecb_checker=False, module=None, detail_level=None, log_path=None,
        flags=None,
    )
    rc = cli.cmd_launch(args)

    assert rc == 0
    assert any(c for c in calls if isinstance(c, tuple) and c[0] == "monitor")


def test_cmd_test_headless_passes_headless_and_restores_after_hide(monkeypatch, capsys):
    calls = []

    _stub_build_deploy(monkeypatch)
    _stub_patch(monkeypatch)
    monkeypatch.setattr(
        cli.launch_mod, "launch",
        lambda **k: (calls.append(("launch", k)) or SimpleNamespace(pid=55, poll=lambda: None)),
    )
    monkeypatch.setattr(cli.launch_mod, "default_timeout", lambda *a: 1)
    monkeypatch.setattr(
        cli.launch_mod, "wait_for_socket",
        lambda **k: {"socket_connected": True, "elapsed_ms": 500},
    )
    monkeypatch.setattr(
        cli.launch_mod, "hide_window",
        lambda: (calls.append("hide") or {"success": True}),
    )
    monkeypatch.setattr(
        cli.launch_mod, "restore_headless_graphics",
        lambda **k: (calls.append(("restore", k)) or {"success": True}),
    )
    monkeypatch.setattr(cli, "run_tests", lambda **k: {"all_passed": True})

    args = argparse.Namespace(
        headless=True, tier=1, filter=None, continue_game=True, save=None,
        skip_videos=True, storylog=False, stats=False, json_mode=False,
        osi_debug=False, syslog=False, modded=False, controller=False,
        ecb_checker=False, module=None, detail_level=None, log_path=None,
        flags=None,
    )
    rc = cli.cmd_test(args)
    assert rc == 0
    launch_call = next(item for item in calls if isinstance(item, tuple) and item[0] == "launch")
    assert launch_call[1]["headless"] is True
    assert "hide" in calls
    assert any(item for item in calls if isinstance(item, tuple) and item[0] == "restore")


def test_cmd_test_headless_retries_retryable_boot_failure(monkeypatch, capsys):
    calls = []
    waits = [
        {"socket_connected": False, "stage": "menu_stalled", "elapsed_ms": 70000},
        {"socket_connected": True, "elapsed_ms": 90000},
    ]
    pids = [55, 56]

    _stub_build_deploy(monkeypatch)
    _stub_patch(monkeypatch)
    monkeypatch.setattr(
        cli.launch_mod, "launch",
        lambda **k: (
            calls.append(("launch", k))
            or SimpleNamespace(pid=pids.pop(0), poll=lambda: None)
        ),
    )
    monkeypatch.setattr(
        cli.launch_mod, "wait_for_socket",
        lambda **k: calls.append("wait") or waits.pop(0),
    )
    monkeypatch.setattr(
        cli.launch_mod, "quit_game",
        lambda **k: calls.append(("quit", k)) or {"success": True, "method": "force"},
    )
    monkeypatch.setattr(
        cli.launch_mod, "restore_headless_graphics",
        lambda **k: calls.append(("restore", k)) or {"success": True},
    )
    monkeypatch.setattr(
        cli.launch_mod, "hide_window",
        lambda: calls.append("hide") or {"success": True},
    )
    monkeypatch.setattr(cli, "run_tests", lambda **k: {"all_passed": True})

    args = argparse.Namespace(
        headless=True, tier=1, filter=None, continue_game=True, save=None,
        timeout=1, boot_retries=1, retry_delay=0,
        skip_videos=True, storylog=False, stats=False, json_mode=False,
        osi_debug=False, syslog=False, modded=False, controller=False,
        ecb_checker=False, module=None, detail_level=None, log_path=None,
        flags=None,
    )
    rc = cli.cmd_test(args)
    output = json.loads(capsys.readouterr().out)

    assert rc == 0
    assert len([c for c in calls if isinstance(c, tuple) and c[0] == "launch"]) == 2
    assert any(c for c in calls if isinstance(c, tuple) and c[0] == "quit")
    assert output["launch"]["pid"] == 56
    assert output["launch"]["boot_retries"] == 1
    assert output["launch"]["boot_attempts"][0]["retrying"] is True
    assert output["launch"]["boot_attempts"][0]["retry_cleanup"]["cancel"]["success"] is True


# ── cmd_build ────────────────────────────────────────────────────────


def test_cmd_build_stops_on_cmake_failure(monkeypatch, capsys):
    monkeypatch.setattr(
        cli.build_mod, "build",
        lambda **k: {"success": False, "error": "SDK missing"},
    )
    args = argparse.Namespace()
    rc = cli.cmd_build(args)
    assert rc == 1


def test_cmd_build_fails_non_universal_binary(monkeypatch, capsys):
    monkeypatch.setattr(cli.build_mod, "build", lambda **k: {"success": True})
    monkeypatch.setattr(
        cli.build_mod, "verify",
        lambda **k: {"verified": False, "arm64": True, "x86_64": False},
    )
    monkeypatch.setattr(cli.build_mod, "deploy", lambda **k: {"deployed": True})
    args = argparse.Namespace()
    rc = cli.cmd_build(args)
    assert rc == 1


# ── cmd_test ─────────────────────────────────────────────────────────


def test_cmd_test_process_exit_is_reported(monkeypatch, capsys):
    _stub_build_deploy(monkeypatch)
    _stub_patch(monkeypatch)
    monkeypatch.setattr(
        cli.launch_mod, "launch",
        lambda **k: SimpleNamespace(pid=55, poll=lambda: 9, returncode=9),
    )
    monkeypatch.setattr(cli.launch_mod, "default_timeout", lambda *a: 1)
    monkeypatch.setattr(
        cli.launch_mod, "wait_for_socket",
        lambda **k: {
            "socket_connected": False,
            "stage": "process_exited",
            "exitcode": 9,
        },
    )

    args = argparse.Namespace(
        headless=False, tier=1, filter=None, continue_game=True, save=None,
        skip_videos=True, storylog=False, stats=False, json_mode=False,
        osi_debug=False, syslog=False, modded=False, controller=False,
        ecb_checker=False, module=None, detail_level=None, log_path=None,
        flags=None,
    )
    rc = cli.cmd_test(args)
    assert rc == 1
