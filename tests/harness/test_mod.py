"""Tier H tests for mod_cli name→UUID resolution.

Regression: mod_cli.py passed raw names to enable_mod() which expects UUIDs.
"""
import json
from types import SimpleNamespace

import pytest


@pytest.fixture
def fake_registry(monkeypatch, tmp_path):
    """Provide a fake mod registry with 3 mods."""
    registry = {
        "aaaa-bbbb-cccc-dddd": {"name": "Mod Configuration Menu", "folder": "MCM"},
        "1111-2222-3333-4444": {"name": "Party Limit Begone", "folder": "PLB"},
        "5555-6666-7777-8888": {"name": "Mod Fixer", "folder": "MF"},
    }
    from bg3se_harness.mod_manager import registry as reg_mod
    monkeypatch.setattr(reg_mod, "load_registry", lambda: registry)
    return registry


def test_resolve_uuid_by_exact_uuid(fake_registry, monkeypatch):
    from bg3se_harness.mod_cli import _resolve_uuid
    assert _resolve_uuid("aaaa-bbbb-cccc-dddd") == "aaaa-bbbb-cccc-dddd"


def test_resolve_uuid_by_name_substring(fake_registry, monkeypatch):
    from bg3se_harness.mod_cli import _resolve_uuid
    assert _resolve_uuid("Configuration Menu") == "aaaa-bbbb-cccc-dddd"


def test_resolve_uuid_case_insensitive(fake_registry, monkeypatch):
    from bg3se_harness.mod_cli import _resolve_uuid
    assert _resolve_uuid("party limit begone") == "1111-2222-3333-4444"


def test_resolve_uuid_ambiguous_returns_error(fake_registry, monkeypatch):
    from bg3se_harness.mod_cli import _resolve_uuid
    result = _resolve_uuid("Mod")
    assert isinstance(result, dict)
    assert "error" in result
    assert "Ambiguous" in result["error"]


def test_resolve_uuid_not_found_returns_error(fake_registry, monkeypatch):
    from bg3se_harness.mod_cli import _resolve_uuid
    result = _resolve_uuid("Nonexistent Mod")
    assert isinstance(result, dict)
    assert "error" in result
    assert "not found" in result["error"]
