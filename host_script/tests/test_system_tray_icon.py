#!/usr/bin/env python3
"""
Test cases for SystemTrayIcon class
"""

import pytest
from unittest.mock import Mock, patch
from PiCoMonitor import SystemTrayIcon


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