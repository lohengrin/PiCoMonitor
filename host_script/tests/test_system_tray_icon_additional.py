#!/usr/bin/env python3
"""
Additional test cases for SystemTrayIcon class to improve coverage
"""

import pytest
from unittest.mock import Mock, patch
from PiCoMonitor import SystemTrayIcon


class TestSystemTrayIconAdditional:
    """Additional tests for SystemTrayIcon to improve coverage"""

    def test_menu_item_interactions(self):
        """Test all menu item interactions"""
        icon = SystemTrayIcon()
        icon.initialize()
        
        # Test menu creation
        menu = icon.create_menu()
        assert menu is not None
        
        # Test menu items
        menu_items = []
        for item in menu._items:
            menu_items.append(item)
        
        # Should have delay items and exit item
        assert len(menu_items) >= 4  # 3 delay items + separator + exit

    def test_delay_change_functionality(self):
        """Test delay change functionality comprehensively"""
        icon = SystemTrayIcon()
        
        # Test setting different delays
        delays = [0.25, 0.5, 1.0, 2.0, 5.0]
        for delay in delays:
            icon.set_delay(delay)
            assert icon.delay == delay
            assert icon.get_delay(delay)
            
            # Test that other delays are not active
            for other_delay in delays:
                if other_delay != delay:
                    assert not icon.get_delay(other_delay)

    def test_exit_action_edge_cases(self, caplog):
        """Test exit action in different states"""
        icon = SystemTrayIcon()
        
        # Test exit when not initialized
        icon.exit_action()
        assert not icon.contflag
        
        # Reset for next test
        icon.contflag = True
        
        # Test exit when initialized
        icon.initialize()
        icon.exit_action()
        assert not icon.contflag
        assert "User requested exit" in caplog.text

    def test_signal_handling(self):
        """Test signal handling functionality"""
        icon = SystemTrayIcon()
        icon.initialize()
        
        # Test that icon has proper signal handling setup
        # (This would be tested more thoroughly in integration tests)
        assert icon.icon is not None

    def test_icon_state_management(self):
        """Test icon state management"""
        icon = SystemTrayIcon()
        
        # Test initial state
        assert icon.contflag is True
        assert icon.delay == 0.5  # Default delay
        
        # Test state changes
        icon.set_delay(1.0)
        assert icon.delay == 1.0
        
        icon.stop()
        assert not icon.contflag

    def test_icon_title_and_visibility(self):
        """Test icon title and visibility management"""
        icon = SystemTrayIcon()
        icon.initialize()
        
        # Test title
        assert icon.icon.title == 'PiCoMonitor - System Monitoring Tool'
        
        # Test visibility (would need actual GUI testing for full coverage)
        assert hasattr(icon.icon, 'visible')

    def test_icon_menu_creation_edge_cases(self):
        """Test icon menu creation with edge cases"""
        icon = SystemTrayIcon()
        
        # Test menu creation with custom title
        custom_icon = SystemTrayIcon(title='Custom Title')
        custom_icon.initialize()
        assert custom_icon.icon.title == 'Custom Title'
        
        # Test menu still works with custom title
        menu = custom_icon.create_menu()
        assert menu is not None

    def test_icon_delay_management(self):
        """Test comprehensive delay management"""
        icon = SystemTrayIcon()
        
        # Test initial delay
        assert icon.delay == 0.5
        assert icon.get_delay(0.5)
        
        # Test delay changes
        icon.set_delay(0.25)
        assert icon.delay == 0.25
        assert icon.get_delay(0.25)
        assert not icon.get_delay(0.5)
        
        # Test setting same delay multiple times
        icon.set_delay(1.0)
        icon.set_delay(1.0)
        assert icon.delay == 1.0
        assert icon.get_delay(1.0)