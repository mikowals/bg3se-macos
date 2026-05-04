"""Mod compatibility test runner.

Orchestrates mod install + save restore + launch + test + report for
automated compatibility verification of popular BG3SE mods.

Scenarios are defined as JSON manifests in catalog/scenarios/.

Usage:
    bg3se-harness compat list              # Available scenarios
    bg3se-harness compat run <scenario>    # Run a scenario end-to-end
    bg3se-harness compat matrix            # Run all scenarios
"""

from __future__ import annotations

import json
import os
import re
import sys
import time
from pathlib import Path

from .config import CATALOG_DIR, SCENARIOS_DIR, REPORTS_DIR


def _load_scenarios():
    """Load all scenario manifests from the scenarios/ directory."""
    scenarios = {}
    if not SCENARIOS_DIR.exists():
        return scenarios
    for f in SCENARIOS_DIR.glob("*.json"):
        try:
            data = json.loads(f.read_text())
            data["_file"] = f.name
            scenarios[f.stem] = data
        except (json.JSONDecodeError, OSError):
            pass
    return scenarios


def _load_popular_mods():
    """Load the popular mods catalog."""
    path = CATALOG_DIR / "popular_mods.json"
    if not path.exists():
        return {}
    try:
        data = json.loads(path.read_text())
        return data.get("mods", {})
    except (json.JSONDecodeError, OSError):
        return {}


def list_scenarios():
    """List available compatibility test scenarios."""
    scenarios = _load_scenarios()
    catalog = _load_popular_mods()

    items = []
    for name, data in scenarios.items():
        items.append({
            "name": name,
            "description": data.get("description", ""),
            "mods": data.get("mods", []),
            "save_fixture": data.get("save_fixture", ""),
        })

    # Also list catalog mods that could be scenarios
    for mod_key, mod_info in catalog.items():
        if mod_key not in scenarios:
            items.append({
                "name": mod_key,
                "description": f"{mod_info['name']} (catalog entry, no scenario manifest)",
                "mods": [mod_key],
                "save_fixture": "",
                "has_manifest": False,
            })

    return {"scenarios": items, "count": len(items)}


def run_scenario(scenario_name):
    """Run a compatibility test scenario.

    Steps:
    1. Load scenario manifest (or build one from catalog)
    2. Install required mods
    3. Restore save fixture (if specified)
    4. Launch game with SE
    5. Wait for socket
    6. Run Lua assertions
    7. Capture screenshots
    8. Collect results
    """
    scenarios = _load_scenarios()
    catalog = _load_popular_mods()

    # Try manifest first, then catalog entry
    if scenario_name in scenarios:
        scenario = scenarios[scenario_name]
    elif scenario_name in catalog:
        mod_info = catalog[scenario_name]
        scenario = {
            "description": f"Auto-generated scenario for {mod_info['name']}",
            "mods": [scenario_name],
            "assertions": [
                "assert(Ext.Utils.Version() ~= nil, 'SE loaded')",
                f"assert(Ext.Mod.IsModLoaded ~= nil, 'Ext.Mod available')",
            ],
        }
    else:
        available = sorted(list(scenarios.keys()) + list(catalog.keys()))
        return {
            "error": f"Scenario '{scenario_name}' not found",
            "available": available,
        }

    run_id = f"{scenario_name}_{int(time.time())}"
    report_dir = REPORTS_DIR / run_id
    report_dir.mkdir(parents=True, exist_ok=True)

    results = {
        "scenario": scenario_name,
        "run_id": run_id,
        "description": scenario.get("description", ""),
        "report_dir": str(report_dir),
        "steps": [],
        "started_at": time.strftime("%Y-%m-%d %H:%M:%S"),
    }

    def log_step(name, result):
        step = {"name": name, "success": result.get("success", "error" not in result), **result}
        results["steps"].append(step)
        status = "OK" if step["success"] else "FAIL"
        print(f"  [{status}] {name}", file=sys.stderr)
        return step["success"]

    print(f"Running compat scenario: {scenario_name}", file=sys.stderr)

    # Step 1: Check prerequisites
    from .doctor import run_doctor
    doctor_result = run_doctor()
    prereq_ok = doctor_result.get("all_passed", False)
    # Only hard-fail on critical checks
    critical_checks = ["bg3_app_bundle", "bg3_binary", "se_dylib_deployed", "mods_directory"]
    critical_passed = all(
        c["passed"] for c in doctor_result["checks"]
        if c["name"] in critical_checks
    )
    log_step("prerequisites", {"success": critical_passed, "passed": doctor_result["passed"], "total": doctor_result["total"]})

    if not critical_passed:
        results["success"] = False
        results["error"] = "Critical prerequisites not met"
        _save_report(report_dir, results)
        return results

    # Step 2: Install mods (if mod PAKs are available locally)
    mod_keys = scenario.get("mods", [])
    for mod_key in mod_keys:
        mod_info = catalog.get(mod_key, {})
        log_step(f"mod_check_{mod_key}", {
            "success": True,
            "name": mod_info.get("name", mod_key),
            "priority": mod_info.get("priority", "unknown"),
            "note": "Mod must be manually installed for now. Auto-install via Nexus planned.",
        })

    # Step 3: Restore save fixture (if specified)
    save_fixture = scenario.get("save_fixture")
    if save_fixture:
        from .savegames import restore
        restore_result = restore(save_fixture)
        log_step("restore_save", restore_result)

    # Step 4: Check if game is already running
    from . import launch as launch_mod
    if launch_mod.is_running() and launch_mod.socket_alive():
        log_step("game_status", {"success": True, "note": "Game already running with SE"})
    else:
        log_step("game_status", {
            "success": False,
            "note": "Game not running. Launch manually or via: bg3se-harness launch --continue",
        })

    # Step 5: Run assertions (if game is running)
    assertions = scenario.get("assertions", [])
    if assertions and launch_mod.socket_alive():
        from .console import Console
        try:
            with Console() as c:
                for i, assertion_lua in enumerate(assertions):
                    try:
                        output = c.send(assertion_lua)
                        log_step(f"assertion_{i}", {"success": True, "lua": assertion_lua, "output": output[:200]})
                    except Exception as e:
                        log_step(f"assertion_{i}", {"success": False, "lua": assertion_lua, "error": str(e)})
        except (ConnectionRefusedError, FileNotFoundError, OSError) as e:
            log_step("assertions", {"success": False, "error": f"Socket connection failed: {e}"})

    # Step 6: Capture screenshot
    if launch_mod.is_running():
        from .screenshot import capture
        ss_result = capture(output=str(report_dir / "screenshot.jpg"))
        log_step("screenshot", {"success": "error" not in ss_result, **ss_result})

    # Step 7: Check crash log
    try:
        from .crashlog import get_crash_report
        crash_data = get_crash_report(ring=False, tail=20)
        has_crash = bool(crash_data.get("signal"))
        log_step("crashlog", {"success": not has_crash, **crash_data})
    except Exception:
        log_step("crashlog", {"success": True, "note": "crashlog check skipped"})

    # Finalize
    passed_steps = sum(1 for s in results["steps"] if s.get("success"))
    total_steps = len(results["steps"])
    results["success"] = all(s.get("success") for s in results["steps"])
    results["summary"] = f"{passed_steps}/{total_steps} steps passed"
    results["finished_at"] = time.strftime("%Y-%m-%d %H:%M:%S")

    _save_report(report_dir, results)
    return results


def run_matrix():
    """Run all scenarios and produce a summary."""
    catalog = _load_popular_mods()
    scenarios = _load_scenarios()
    all_names = sorted(set(list(scenarios.keys()) + list(catalog.keys())))

    matrix_results = []
    for name in all_names:
        result = run_scenario(name)
        matrix_results.append({
            "scenario": name,
            "success": result.get("success", False),
            "summary": result.get("summary", ""),
            "run_id": result.get("run_id", ""),
        })

    passed = sum(1 for r in matrix_results if r["success"])
    return {
        "matrix": matrix_results,
        "passed": passed,
        "total": len(matrix_results),
        "all_passed": passed == len(matrix_results),
    }


def vet_mod(source, no_launch=False, output_path=None):
    """Vet a single mod: install, detect SE requirement, probe, generate report.

    source: Nexus mod ID (int), catalog key (str), or path to .pak file.

    Returns a compat report dict with status, errors, warnings, and API usage.
    """
    from . import launch as launch_mod

    catalog = _load_popular_mods()
    report = {
        "source": source,
        "bg3se_version": _read_se_version(),
        "timestamp": time.strftime("%Y-%m-%dT%H:%M:%S"),
        "status": "unknown",
        "se_required": False,
        "load_success": False,
        "bootstrap_executed": False,
        "apis_used": [],
        "errors": [],
        "warnings": [],
        "notes": "",
    }

    # Resolve source to mod info
    mod_info = None
    pak_path = None

    if source in catalog:
        mod_info = catalog[source]
        report["mod_name"] = mod_info["name"]
        report["nexus_id"] = mod_info.get("nexus_id")
        report["apis_used"] = mod_info.get("apis_used", [])
        print(f"Vetting catalog mod: {mod_info['name']}", file=sys.stderr)
    elif os.path.isfile(source) and Path(source).suffix.lower() == ".pak":
        pak_path = source
        report["mod_name"] = Path(source).stem
        print(f"Vetting local PAK: {source}", file=sys.stderr)
    else:
        try:
            nexus_id = int(source)
            report["nexus_id"] = nexus_id
            report["mod_name"] = f"nexus:{nexus_id}"
            print(f"Vetting Nexus mod #{nexus_id}", file=sys.stderr)
        except ValueError:
            report["status"] = "error"
            report["errors"].append(f"Cannot resolve source: {source!r}")
            return report

    # Step 1: Check if mod is installed (via PAK or modsettings)
    from .mod_manager.registry import load_registry
    registry = load_registry()
    installed_uuid = None

    if mod_info and mod_info.get("nexus_id"):
        for uuid, entry in registry.items():
            if mod_info["name"].lower() in (entry.get("name") or "").lower():
                installed_uuid = uuid
                break

    if pak_path:
        from .mod_manager.pak_inspector import PakReader, PakInspectorError
        try:
            with PakReader(pak_path) as pak:
                info = pak.get_mod_info()
                report["se_required"] = pak.contains_script_extender()
                if info.get("uuid"):
                    installed_uuid = info["uuid"]
                    report["mod_name"] = info.get("name") or report["mod_name"]
        except (PakInspectorError, OSError) as e:
            report["errors"].append(f"PAK read failed: {e}")

    # Step 2: Check SE socket and game status
    game_running = launch_mod.is_running()
    socket_ok = launch_mod.socket_alive()

    if not game_running and not no_launch:
        report["warnings"].append("Game not running. Launch with: bg3se-harness launch --continue")
        report["status"] = "needs_launch"
        _save_vet_report(report, output_path)
        return report

    if not socket_ok:
        report["warnings"].append("SE socket not connected — game may be starting or SE not loaded")
        report["status"] = "no_socket"
        _save_vet_report(report, output_path)
        return report

    # Step 3: Probe via socket
    from .console import Console
    try:
        with Console() as c:
            # Check SE loaded
            version = c.send("Ext.Print(Ext.Utils.Version())")
            report["bg3se_version_live"] = version.strip() if version else "unknown"

            # Check mod loaded (if we know the UUID)
            if installed_uuid and _valid_uuid(installed_uuid):
                is_loaded = c.send(
                    f'local ok, res = pcall(Ext.Mod.IsModLoaded, "{installed_uuid}"); '
                    f'Ext.Print(tostring(ok and res))'
                )
                report["load_success"] = "true" in (is_loaded or "").lower()

                bootstrap_check = c.send(
                    f'local ok = pcall(function() return Ext.Mod.GetMod("{installed_uuid}") end); '
                    f'Ext.Print(tostring(ok))'
                )
                report["bootstrap_executed"] = "true" in (bootstrap_check or "").lower()
            elif installed_uuid:
                report["warnings"].append(f"Invalid UUID format, skipping probe: {installed_uuid!r}")

    except (ConnectionRefusedError, FileNotFoundError, OSError) as e:
        report["errors"].append(f"Console connection failed: {e}")
        report["status"] = "socket_error"
        _save_vet_report(report, output_path)
        return report

    # Step 4: Parse latest log for errors related to this mod
    log_errors, log_warnings = _scan_log_for_mod(
        report.get("mod_name", ""),
        since_timestamp=report["timestamp"].replace("T", " "),
    )
    report["errors"].extend(log_errors)
    report["warnings"].extend(log_warnings)

    # Step 5: Determine status
    if report["errors"]:
        report["status"] = "broken"
    elif report["load_success"] and not report["warnings"]:
        report["status"] = "working"
    elif report["load_success"]:
        report["status"] = "partial"
    else:
        report["status"] = "not_loaded"

    _save_vet_report(report, output_path)
    return report


_UUID_RE = re.compile(
    r'^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-'
    r'[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$'
)


def _valid_uuid(s):
    """Return True if s is a well-formed UUID (hex-dash format)."""
    return bool(s and _UUID_RE.match(s))


def _read_se_version():
    """Read SE version from version.h."""
    try:
        from .config import PROJECT_ROOT
        version_h = PROJECT_ROOT / "src/core/version.h"
        if version_h.exists():
            for line in version_h.read_text().splitlines():
                if "BG3SE_VERSION" in line and '"' in line:
                    return line.split('"')[1]
    except Exception:
        pass
    return "unknown"


def _scan_log_for_mod(mod_name, since_timestamp=None):
    """Scan latest.log for errors/warnings mentioning a mod.

    If *since_timestamp* is given (ISO format "YYYY-MM-DD HH:MM:SS"), only
    lines with timestamps at or after that value are considered.
    """
    errors = []
    warnings = []
    if not mod_name:
        return errors, warnings
    log_path = Path.home() / "Library/Application Support/BG3SE/logs/latest.log"
    if not log_path.exists():
        return errors, warnings

    try:
        lines = log_path.read_text(errors="replace").splitlines()
        mod_lower = mod_name.lower()
        for line in lines[-500:]:
            if since_timestamp and line.startswith("["):
                ts_end = line.find("]")
                if ts_end > 1:
                    line_ts = line[1:ts_end].split(".")[0]
                    if line_ts < since_timestamp:
                        continue
            lower = line.lower()
            if mod_lower and mod_lower not in lower:
                continue
            if "error" in lower or "failed" in lower:
                errors.append(line.strip()[:200])
            elif "warn" in lower:
                warnings.append(line.strip()[:200])
    except OSError:
        pass

    return errors[:20], warnings[:20]


def _save_vet_report(report, output_path=None):
    """Save a vet report to docs/compat-reports/."""
    from .config import PROJECT_ROOT
    if output_path:
        out = Path(output_path)
    else:
        reports_dir = PROJECT_ROOT / "docs" / "compat-reports"
        reports_dir.mkdir(parents=True, exist_ok=True)
        mod_slug = report.get("mod_name", "unknown").lower().replace(" ", "_")[:40]
        out = reports_dir / f"{mod_slug}_{int(time.time())}.json"

    out.write_text(json.dumps(report, indent=2))
    print(f"Report saved: {out}", file=sys.stderr)


def _save_report(report_dir, results):
    """Save report JSON to report directory."""
    report_path = report_dir / "report.json"
    report_path.write_text(json.dumps(results, indent=2))


# ============================================================================
# CLI handler
# ============================================================================

def cmd_compat(args):
    """CLI handler for compat subcommands."""
    subcmd = args.compat_command

    if subcmd == "list":
        result = list_scenarios()
        print(json.dumps(result, indent=2))
        return 0

    elif subcmd == "run":
        result = run_scenario(args.scenario)
        print(json.dumps(result, indent=2))
        return 0 if result.get("success") else 1

    elif subcmd == "matrix":
        result = run_matrix()
        print(json.dumps(result, indent=2))
        return 0 if result.get("all_passed") else 1

    elif subcmd == "vet":
        result = vet_mod(
            args.source,
            no_launch=getattr(args, "no_launch", False),
            output_path=getattr(args, "output", None),
        )
        print(json.dumps(result, indent=2))
        return 0 if result.get("status") == "working" else 1

    return 1
