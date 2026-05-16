"""Offline tests for BG3 menu geometry and click diagnostics."""

import argparse
import json

from bg3se_harness import menu


def test_coordinate_debug_uses_system_events_retina_scale():
    bbox = {"x": 0.4, "y": 0.4, "width": 0.2, "height": 0.2}
    system_bounds = {"x": 10, "y": 20, "width": 1000, "height": 500}
    quartz_bounds = {"x": 10, "y": 20, "width": 2000, "height": 1000}

    result = menu._coordinate_debug_for_bbox(
        bbox, img_w=2000, img_h=1000,
        system_bounds=system_bounds, quartz_bounds=quartz_bounds,
    )

    assert result["pixel_center"] == {"x": 1000, "y": 500}
    assert result["selected_basis"] == "system_events_points"
    assert result["selected"] == {"x": 510, "y": 270}
    assert result["candidates"]["system_events_points"]["scale_x"] == 2
    assert result["candidates"]["system_events_points"]["scale_y"] == 2
    assert result["candidates"]["system_events_points"]["inside"] is True


def test_coordinate_debug_falls_back_to_quartz_bounds():
    bbox = {"x": 0.25, "y": 0.25, "width": 0.5, "height": 0.5}
    quartz_bounds = {"x": 0, "y": 0, "width": 1280, "height": 720}

    result = menu._coordinate_debug_for_bbox(
        bbox, img_w=1280, img_h=720,
        system_bounds=None, quartz_bounds=quartz_bounds,
    )

    assert result["selected_basis"] == "quartz_bounds_scaled"
    assert result["selected"] == {"x": 640, "y": 360}
    assert result["candidates"]["quartz_bounds_scaled"]["inside"] is True


def test_click_menu_button_reports_coordinate_basis(monkeypatch):
    detection = {
        "buttons": [{
            "text": "Continue",
            "screen_x": 510,
            "screen_y": 270,
            "coordinate_debug": {"selected_basis": "system_events_points"},
        }],
        "geometry": {"window_id": 42},
    }
    monkeypatch.setattr(menu, "detect_menu", lambda: detection)
    monkeypatch.setattr(menu, "cg_click", lambda x, y: (x, y) == (510, 270))
    monkeypatch.setattr(menu, "activate_bg3", lambda: {"success": True})
    monkeypatch.setattr(menu.time, "sleep", lambda _: None)

    result = menu.click_menu_button("Continue")

    assert result["success"] is True
    assert result["coordinate_basis"] == "system_events_points"
    assert result["geometry"] == {"window_id": 42}


def test_click_fraction_uses_window_bounds_and_both_methods(monkeypatch):
    calls = []
    monkeypatch.setattr(
        menu,
        "collect_geometry",
        lambda capture=False: {
            "system_events": {
                "bounds": {"x": 100, "y": 200, "width": 1000, "height": 500},
            },
        },
    )
    monkeypatch.setattr(
        menu,
        "activate_bg3",
        lambda: {"success": True, "method": "test_activate"},
    )
    monkeypatch.setattr(menu.time, "sleep", lambda _: None)
    monkeypatch.setattr(
        menu,
        "cg_click",
        lambda x, y: calls.append(("cg", x, y)) or True,
    )
    monkeypatch.setattr(
        menu,
        "system_events_click",
        lambda x, y: calls.append(("se", x, y)) or {
            "success": True,
            "method": "SystemEvents_click_at",
        },
    )

    result = menu.click_fraction(0.25, 0.5, method="both", activate=True)

    assert result["success"] is True
    assert result["point"] == {"x": 350, "y": 450}
    assert result["basis"] == "system_events_bounds"
    assert calls == [("cg", 350, 450), ("se", 350, 450)]


def test_cmd_menu_geometry_prints_json(monkeypatch, capsys):
    monkeypatch.setattr(
        menu, "collect_geometry",
        lambda capture=False: {"pid": 123, "window_id": 456, "capture": capture},
    )

    rc = menu.cmd_menu(argparse.Namespace(menu_command="geometry", capture=True))
    output = json.loads(capsys.readouterr().out)

    assert rc == 0
    assert output == {"pid": 123, "window_id": 456, "capture": True}
