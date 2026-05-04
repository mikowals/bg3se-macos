"""Tests for bg3se_harness.cli — cmd_launch, cmd_build, cmd_test."""

import argparse
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
        lambda **k: (calls.append("launch") or SimpleNamespace(pid=123, poll=lambda: None)),
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

    args = argparse.Namespace(
        headless=True, timeout=1, continue_game=False, save=None,
        skip_videos=True, storylog=False, stats=False, json_mode=False,
        osi_debug=False, syslog=False, modded=False, controller=False,
        ecb_checker=False, module=None, detail_level=None, log_path=None,
        flags=None,
    )
    rc = cli.cmd_launch(args)
    assert rc == 1


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
