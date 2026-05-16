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


def test_cmd_mod_enable_adds_missing_registry_mod(fake_registry, monkeypatch, capsys):
    from bg3se_harness import mod_cli
    from bg3se_harness.mod_manager import modsettings, registry

    calls = {}

    def fake_add_mod(**kwargs):
        calls["add_mod"] = kwargs
        return {"added": True, "uuid": kwargs["uuid"], "name": kwargs["name"]}

    def fake_set_enabled(uuid, enabled):
        calls["set_enabled"] = (uuid, enabled)
        return {"updated": True}

    monkeypatch.setattr(modsettings, "add_mod", fake_add_mod)
    monkeypatch.setattr(registry, "set_mod_enabled", fake_set_enabled)

    args = SimpleNamespace(mod_command="enable", name="Configuration Menu")
    assert mod_cli.cmd_mod(args) == 0

    output = json.loads(capsys.readouterr().out)
    assert output["enabled"] is True
    assert calls["add_mod"] == {
        "uuid": "aaaa-bbbb-cccc-dddd",
        "name": "Mod Configuration Menu",
        "folder": "MCM",
        "version": "36028797018963968",
        "md5": "",
    }
    assert calls["set_enabled"] == ("aaaa-bbbb-cccc-dddd", True)


def test_list_mods_reports_actual_load_order_state(monkeypatch):
    from bg3se_harness.mod_manager import registry
    from bg3se_harness.mod_manager import modsettings

    monkeypatch.setattr(registry, "load_registry", lambda: {
        "active-uuid": {
            "uuid": "active-uuid",
            "name": "Active Mod",
            "enabled": True,
        },
        "reset-uuid": {
            "uuid": "reset-uuid",
            "name": "Reset Mod",
            "enabled": True,
        },
    })
    monkeypatch.setattr(modsettings, "read_mod_order", lambda: [
        {"uuid": "active-uuid", "name": "Active Mod"},
    ])

    mods = registry.list_mods()
    by_uuid = {m["uuid"]: m for m in mods}

    assert by_uuid["active-uuid"]["enabled"] is True
    assert by_uuid["active-uuid"]["registered_enabled"] is True
    assert by_uuid["reset-uuid"]["enabled"] is False
    assert by_uuid["reset-uuid"]["registered_enabled"] is True
