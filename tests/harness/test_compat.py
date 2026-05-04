"""Tier H tests for compat._scan_log_for_mod timestamp scoping.

Regression: _scan_log_for_mod read last 500 lines without filtering
by timestamp, so stale errors from prior sessions polluted vet reports.
"""
import pytest
from pathlib import Path


LOG_LINES = [
    "[2026-05-01 10:00:00.000] [ERROR] [Mod    ] TestMod: old error from days ago",
    "[2026-05-01 10:00:01.000] [WARN ] [Mod    ] TestMod: old warning",
    "[2026-05-03 14:00:00.000] [ERROR] [Mod    ] TestMod: fresh error",
    "[2026-05-03 14:00:01.000] [WARN ] [Mod    ] TestMod: fresh warning",
    "[2026-05-03 14:00:02.000] [INFO ] [Mod    ] TestMod: loaded successfully",
]


@pytest.fixture
def fake_home(monkeypatch, tmp_path):
    """Redirect Path.home() so _scan_log_for_mod finds our fake log."""
    log_dir = tmp_path / "Library" / "Application Support" / "BG3SE" / "logs"
    log_dir.mkdir(parents=True)
    log_path = log_dir / "latest.log"
    log_path.write_text("\n".join(LOG_LINES))

    monkeypatch.setattr(Path, "home", classmethod(lambda cls: tmp_path))
    return log_path


def test_scan_without_timestamp_returns_all(fake_home):
    from bg3se_harness.compat import _scan_log_for_mod
    errors, warnings = _scan_log_for_mod("TestMod")
    assert len(errors) == 2
    assert len(warnings) == 2


def test_scan_with_timestamp_filters_old(fake_home):
    from bg3se_harness.compat import _scan_log_for_mod
    errors, warnings = _scan_log_for_mod("TestMod", since_timestamp="2026-05-03 00:00:00")
    assert len(errors) == 1
    assert "fresh error" in errors[0]
    assert len(warnings) == 1
    assert "fresh warning" in warnings[0]


def test_scan_with_future_timestamp_returns_none(fake_home):
    from bg3se_harness.compat import _scan_log_for_mod
    errors, warnings = _scan_log_for_mod("TestMod", since_timestamp="2027-01-01 00:00:00")
    assert len(errors) == 0
    assert len(warnings) == 0
