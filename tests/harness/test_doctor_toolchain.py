"""Tier H tests for the doctor toolchain checks (#88).

Every check runs against an injected `run` so no real toolchain is needed;
each test shapes one failure the issue reporters could hit.
"""

from __future__ import annotations

import subprocess
from types import SimpleNamespace

import pytest

from bg3se_harness.doctor import (
    TOOLCHAIN_FIX,
    TOOLCHAIN_PROBE_SOURCE,
    toolchain_checks,
)


DEVELOPER_DIR = "/Library/Developer/CommandLineTools"
SDK_PATH = "/Library/Developer/CommandLineTools/SDKs/MacOSX15.sdk"
CLANG = "/Library/Developer/CommandLineTools/usr/bin/clang++"
TUPLE_ERROR = (
    f"{SDK_PATH}/usr/include/simd/vector_make.h:5922:10: "
    "fatal error: 'tuple' file not found"
)


def _completed(rc=0, stdout="", stderr=""):
    return SimpleNamespace(returncode=rc, stdout=stdout, stderr=stderr)


def _healthy_runner(overrides=None, calls=None):
    """Build a fake subprocess.run keyed on the command name."""
    responses = {
        ("xcode-select", "-p"): _completed(0, DEVELOPER_DIR + "\n"),
        ("xcrun", "--sdk", "macosx", "--show-sdk-path"): _completed(0, SDK_PATH + "\n"),
        ("xcrun", "--find", "clang++"): _completed(0, CLANG + "\n"),
        (CLANG, "--version"): _completed(0, "Apple clang version 17.0.0\nTarget: arm64\n"),
        "compile": _completed(0),
        ("cmake", "--version"): _completed(0, "cmake version 4.2.0\n"),
    }
    responses.update(overrides or {})

    def run(argv, **kwargs):
        if calls is not None:
            calls.append((tuple(argv), kwargs))
        if argv[0] == CLANG and "-fsyntax-only" in argv:
            return responses["compile"]
        return responses[tuple(argv)]

    return run


def _by_name(checks):
    return {c["name"]: c for c in checks}


def test_healthy_toolchain_passes_every_check():
    calls = []
    checks = _by_name(toolchain_checks(run=_healthy_runner(calls=calls), path_exists=lambda p: True))

    assert len(checks) == 5
    assert all(c["passed"] for c in checks.values())
    assert all(c["severity"] == "warning" for c in checks.values())
    assert all("code" not in c for c in checks.values())
    assert checks["toolchain_developer_dir"]["detail"] == DEVELOPER_DIR
    assert checks["toolchain_macos_sdk"]["detail"] == SDK_PATH
    assert checks["toolchain_objcxx_compiler"]["detail"] == f"{CLANG} (Apple clang version 17.0.0)"
    assert checks["toolchain_cmake"]["detail"] == "cmake version 4.2.0"

    # The include probe compiles the same chain CMake probes, against the SDK xcrun reported.
    compile_calls = [c for c in calls if c[0][0] == CLANG and "-fsyntax-only" in c[0]]
    assert len(compile_calls) == 1
    argv, kwargs = compile_calls[0]
    assert "-isysroot" in argv and argv[argv.index("-isysroot") + 1] == SDK_PATH
    assert argv[argv.index("-x") + 1] == "objective-c++"
    assert kwargs["input"] == TOOLCHAIN_PROBE_SOURCE
    assert "#include <tuple>" in TOOLCHAIN_PROBE_SOURCE
    assert "MetalKit/MetalKit.h" in TOOLCHAIN_PROBE_SOURCE


def test_tuple_not_found_is_reported_with_the_compiler_line():
    run = _healthy_runner({"compile": _completed(1, "", TUPLE_ERROR + "\n1 error generated.")})
    checks = _by_name(toolchain_checks(run=run, path_exists=lambda p: True))

    include = checks["toolchain_objcxx_includes"]
    assert include["passed"] is False
    assert include["code"] == "toolchain_objcxx_includes"
    assert include["detail"] == TUPLE_ERROR
    assert "CMAKE_OSX_SYSROOT" in include["fix"]
    assert include["fix"] == TOOLCHAIN_FIX
    # The other checks stay green: the defect is the include chain, not the tools.
    assert checks["toolchain_developer_dir"]["passed"]
    assert checks["toolchain_macos_sdk"]["passed"]
    assert checks["toolchain_objcxx_compiler"]["passed"]


def test_broken_developer_dir_fails_only_that_check():
    run = _healthy_runner({
        ("xcode-select", "-p"): _completed(
            2, "", "xcode-select: error: unable to get active developer directory"
        ),
    })
    checks = _by_name(toolchain_checks(run=run, path_exists=lambda p: True))

    dev = checks["toolchain_developer_dir"]
    assert dev["passed"] is False
    assert dev["code"] == "toolchain_developer_dir"
    assert "unable to get active developer directory" in dev["detail"]
    assert "xcode-select -s" in dev["fix"]
    assert checks["toolchain_objcxx_includes"]["passed"]


def test_missing_sdk_directory_fails_sdk_and_skips_the_compile():
    calls = []
    run = _healthy_runner(calls=calls)
    checks = _by_name(toolchain_checks(run=run, path_exists=lambda p: p != SDK_PATH))

    assert checks["toolchain_macos_sdk"]["passed"] is False
    assert checks["toolchain_macos_sdk"]["detail"] == SDK_PATH
    include = checks["toolchain_objcxx_includes"]
    assert include["passed"] is False
    assert include["detail"] == "skipped: compiler or SDK unavailable"
    assert not any(c[0][0] == CLANG and "-fsyntax-only" in c[0] for c in calls)


def test_absent_tools_degrade_to_failures_not_exceptions():
    def run(argv, **kwargs):
        raise FileNotFoundError(argv[0])

    checks = _by_name(toolchain_checks(run=run, path_exists=lambda p: True))

    assert len(checks) == 5
    assert not any(c["passed"] for c in checks.values())
    assert checks["toolchain_developer_dir"]["detail"] == "xcode-select: not found"
    assert checks["toolchain_cmake"]["detail"] == "cmake: not found"
    assert checks["toolchain_cmake"]["fix"].startswith("brew install cmake")


def test_compile_timeout_is_a_failure_with_reason():
    def run(argv, **kwargs):
        if argv[0] == CLANG and "-fsyntax-only" in argv:
            raise subprocess.TimeoutExpired(argv, kwargs.get("timeout", 60))
        return _healthy_runner()(argv, **kwargs)

    checks = _by_name(toolchain_checks(run=run, path_exists=lambda p: True))
    include = checks["toolchain_objcxx_includes"]
    assert include["passed"] is False
    assert include["detail"].startswith(f"{CLANG}: timed out")


def test_run_doctor_includes_toolchain_checks_as_warnings(monkeypatch):
    from bg3se_harness import doctor

    monkeypatch.setattr(doctor, "toolchain_checks", lambda: [
        doctor._check("toolchain_objcxx_includes", False, severity="warning",
                      code="toolchain_objcxx_includes", fix=TOOLCHAIN_FIX),
    ])
    # Keep the rest of run_doctor hermetic enough to reach the summary.
    monkeypatch.setattr(doctor.launch_mod, "is_running", lambda: False)
    monkeypatch.setattr(doctor.launch_mod, "socket_alive", lambda: False)
    monkeypatch.setattr(doctor.launch_mod, "steam_readiness", lambda: {"status": "ready"})
    monkeypatch.setattr(doctor.launch_mod, "memory_pressure_check",
                        lambda: {"classification": "pass", "free_percent": 50})
    monkeypatch.setattr(doctor.launch_mod, "check_windowed_mode", lambda: {"windowed": True})
    monkeypatch.setattr(doctor.patch_mod, "is_patched", lambda: True)
    monkeypatch.setattr(doctor.subprocess, "run",
                        lambda *a, **k: SimpleNamespace(returncode=0, stdout="1\n", stderr=""))

    result = doctor.run_doctor()
    names = [c["name"] for c in result["checks"]]
    assert names[-1] == "toolchain_objcxx_includes"
    # A toolchain failure warns; it never blocks a launch.
    assert "toolchain_objcxx_includes" not in result["failed_critical"]
