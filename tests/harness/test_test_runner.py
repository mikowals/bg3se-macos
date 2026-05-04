"""Tests for bg3se_harness.test_runner — parse_test_output and run_tests."""

import pytest

from bg3se_harness.test_runner import parse_test_output


def test_parse_test_output_accepts_slow_token():
    raw = (
        "  PASS: Core.Slow (1200ms) [SLOW 1000ms] [1/1]\n"
        "=== Results: 1/1 passed, 0 failed, 0 skipped (1200ms) ==="
    )
    tests, summary = parse_test_output(raw)
    assert len(tests) == 1
    assert tests[0]["name"] == "Core.Slow"
    assert tests[0]["status"] == "pass"
    assert tests[0]["ms"] == 1200
    assert tests[0]["error"] is None
    assert tests[0]["index"] == 1
    assert tests[0]["total"] == 1
    assert summary["passed"] == 1
    assert summary["failed"] == 0


def test_parse_test_output_derives_summary_when_missing():
    raw = (
        "  PASS: Core.Print (2ms) [1/2]\n"
        "  FAIL: Core.Bad (3ms) - nope [2/2]"
    )
    tests, summary = parse_test_output(raw)
    assert summary is None
    assert len(tests) == 2
    assert tests[0]["status"] == "pass"
    assert tests[1]["status"] == "fail"
    assert tests[1]["error"] == "nope"


def test_parse_test_output_empty_returns_no_tests():
    tests, summary = parse_test_output("")
    assert tests == []
    assert summary is None


def test_parse_test_output_ignores_unrelated_lines():
    raw = (
        "Loading mods...\n"
        "  PASS: Stats.Get (5ms) [1/1]\n"
        "Some random log line\n"
        "=== Results: 1/1 passed, 0 failed, 0 skipped (5ms) ==="
    )
    tests, summary = parse_test_output(raw)
    assert len(tests) == 1
    assert summary["passed"] == 1


def test_run_tests_returns_structured_socket_error(monkeypatch):
    from bg3se_harness import test_runner

    class BrokenConsole:
        def __init__(self, *a, **k):
            pass
        def __enter__(self):
            raise FileNotFoundError("/tmp/bg3se.sock")
        def __exit__(self, *a):
            pass

    monkeypatch.setattr(test_runner, "Console", BrokenConsole)
    result = test_runner.run_tests(tier=1)
    assert result["all_passed"] is False
    assert result["tests"] == []
    assert "Socket connection failed" in result["error"]


def test_run_tests_empty_output_is_not_all_passed(monkeypatch):
    from bg3se_harness import test_runner

    class EmptyConsole:
        def __init__(self, *a, **k):
            pass
        def __enter__(self):
            return self
        def __exit__(self, *a):
            pass
        def send(self, cmd, timeout=None):
            return ""

    monkeypatch.setattr(test_runner, "Console", EmptyConsole)
    result = test_runner.run_tests(tier=1)
    assert result["all_passed"] is False
    assert result["tests"] == []
