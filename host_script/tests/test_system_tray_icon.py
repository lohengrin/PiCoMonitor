#!/usr/bin/env python3
"""
Test cases for SystemTrayIcon class
"""

import os
import sys

import pytest
from types import SimpleNamespace
from unittest.mock import Mock, patch
from PiCoMonitor import (SystemTrayIcon, _TRAY_ICON_NAME, _appindicator_icon_class,
                          _install_tray_theme_icon, warn_if_tray_backend_unsupported)


class TestSystemTrayIcon:
    """Test SystemTrayIcon functionality"""

    def test_create_image(self):
        """Test image creation"""
        icon = SystemTrayIcon()
        image = icon.create_image(64, 64, 'blue', 'white')
        assert image is not None
        assert image.size == (64, 64)

    def test_set_delay(self):
        """Test setting delay"""
        icon = SystemTrayIcon()
        icon.set_delay(1.0)
        assert icon.delay == 1.0
        assert icon.get_delay(1.0)

    def test_get_delay(self):
        """Test getting delay"""
        icon = SystemTrayIcon()
        icon.delay = 0.5
        assert icon.get_delay(0.5)
        assert not icon.get_delay(1.0)

    def test_exit_action(self, caplog):
        """Test exit action"""
        icon = SystemTrayIcon()
        icon.initialize()
        icon.exit_action()
        assert not icon.contflag
        assert "User requested exit" in caplog.text

    def test_initialize(self):
        """Test icon initialization"""
        icon = SystemTrayIcon()
        icon.initialize()
        assert icon.icon is not None
        assert icon.icon.title == 'PiCoMonitor - System Monitoring Tool'


class TestTrayBackendWarning:
    """The Xorg backend has no tray under Wayland (Raspberry Pi OS): warn instead of
    silently showing no icon."""

    @staticmethod
    def _fake_pystray(module: str):
        icon_cls = type("Icon", (), {"__module__": module})
        return SimpleNamespace(Icon=icon_cls)

    def test_warns_for_xorg_under_wayland(self, caplog):
        with patch("PiCoMonitor.pystray", self._fake_pystray("pystray._xorg")), \
                patch.object(sys, "platform", "linux"), \
                patch.dict(os.environ, {"WAYLAND_DISPLAY": "wayland-0"}):
            warn_if_tray_backend_unsupported()
        assert "Xorg backend" in caplog.text

    def test_no_warning_for_appindicator(self, caplog):
        with patch("PiCoMonitor.pystray", self._fake_pystray("pystray._appindicator")), \
                patch.object(sys, "platform", "linux"), \
                patch.dict(os.environ, {"WAYLAND_DISPLAY": "wayland-0"}):
            warn_if_tray_backend_unsupported()
        assert "Xorg backend" not in caplog.text

    def test_no_warning_without_wayland(self, caplog):
        with patch("PiCoMonitor.pystray", self._fake_pystray("pystray._xorg")), \
                patch.object(sys, "platform", "linux"), \
                patch.dict(os.environ, {}, clear=True):
            warn_if_tray_backend_unsupported()
        assert "Xorg backend" not in caplog.text

    def test_no_warning_without_pystray(self, caplog):
        with patch("PiCoMonitor.pystray", None), \
                patch.dict(os.environ, {"WAYLAND_DISPLAY": "wayland-0"}):
            warn_if_tray_backend_unsupported()
        assert "Xorg backend" not in caplog.text

class TestTrayThemeIcon:
    """Raspberry Pi OS's panel (wf-panel-pi) can only load icons from the icon theme, so the
    tray icon is installed there and published by name instead of pystray's temp-file path."""

    def test_install_writes_pngs_into_the_theme(self, tmp_path):
        with patch("PiCoMonitor.subprocess.run") as run:
            _install_tray_theme_icon(base=str(tmp_path))
        for size in ("32x32", "64x64"):
            icon = tmp_path / size / "apps" / f"{_TRAY_ICON_NAME}.png"
            assert icon.is_file()
            from PIL import Image
            assert Image.open(icon).size == (int(size[:2]), int(size[:2]))
        run.assert_called_once()
        assert run.call_args[0][0][0] == "gtk-update-icon-cache"

    def test_install_without_cache_tool_is_not_fatal(self, tmp_path):
        with patch("PiCoMonitor.subprocess.run", side_effect=OSError("no gtk-update-icon-cache")):
            _install_tray_theme_icon(base=str(tmp_path))       # must not raise
        assert (tmp_path / "64x64" / "apps" / f"{_TRAY_ICON_NAME}.png").is_file()

    def test_other_backends_are_returned_unchanged(self):
        fake = SimpleNamespace(Icon=type("Icon", (), {"__module__": "pystray._win32"}))
        with patch("PiCoMonitor.pystray", fake), patch("PiCoMonitor._tray_icon_class_cache", None):
            assert _appindicator_icon_class() is fake.Icon

    def test_appindicator_backend_gets_a_themed_subclass(self):
        """On Linux with the AppIndicator backend the icon must be a subclass that publishes
        the theme name, and the theme icon must be installed."""
        installed = []
        fake = SimpleNamespace(Icon=type("Icon", (), {"__module__": "pystray._appindicator"}))
        with patch("PiCoMonitor.pystray", fake), \
                patch("PiCoMonitor._tray_icon_class_cache", None), \
                patch.object(sys, "platform", "linux"), \
                patch("PiCoMonitor._install_tray_theme_icon",
                      lambda *a, **k: installed.append(True)):
            cls = _appindicator_icon_class()
        from pystray._appindicator import Icon as pystray_appindicator_icon
        assert cls is not fake.Icon
        assert issubclass(cls, pystray_appindicator_icon)
        assert installed                                    # theme icon installed for it
        assert cls._update_icon and cls._show               # the overridden hooks exist
