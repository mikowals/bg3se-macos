"""Tier H tests for savegames.restore() backup behavior.

Regression: restore() destroyed existing saves via shutil.rmtree without backup.
"""
import shutil
from pathlib import Path

import pytest


@pytest.fixture
def save_env(monkeypatch, tmp_path):
    """Set up fake save dirs and fixture."""
    saves_dir = tmp_path / "saves"
    saves_dir.mkdir()
    fixtures_dir = tmp_path / "fixtures"
    fixtures_dir.mkdir()

    fixture = fixtures_dir / "test_fixture"
    fixture.mkdir()
    (fixture / "save.lsv").write_text("fixture_data")

    import bg3se_harness.savegames as sg
    monkeypatch.setattr(sg, "SAVES_DIR", saves_dir)
    monkeypatch.setattr(sg, "SAVE_FIXTURES_DIR", fixtures_dir)
    return saves_dir, fixtures_dir


def test_restore_backs_up_existing_save(save_env):
    saves_dir, _ = save_env
    import bg3se_harness.savegames as sg

    existing = saves_dir / "Harness__test_fixture"
    existing.mkdir()
    (existing / "save.lsv").write_text("original_data")

    result = sg.restore("test_fixture")
    assert result.get("success") is True

    backups = [d for d in saves_dir.iterdir() if "__backup_" in d.name]
    assert len(backups) == 1
    assert (backups[0] / "save.lsv").read_text() == "original_data"

    assert (existing / "save.lsv").read_text() == "fixture_data"


def test_restore_works_without_existing_save(save_env):
    saves_dir, _ = save_env
    import bg3se_harness.savegames as sg

    result = sg.restore("test_fixture")
    assert result.get("success") is True

    dest = saves_dir / "Harness__test_fixture"
    assert dest.exists()
    assert (dest / "save.lsv").read_text() == "fixture_data"
