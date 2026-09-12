#!/usr/bin/env python3
"""
Test cases for Application class
"""

import pytest
import sys
from unittest.mock import Mock, patch
from PiCoMonitor import Application


class TestApplication:
    """Test Application functionality"""

    def test_parse_arguments(self):
        """Test argument parsing"""
        with patch('sys.argv', ['PiCoMonitor.py', '--delay', '0.5', '--port', 'COM5']):
            app = Application()
            app.parse_arguments()
            assert app.args.delay == 0.5
            assert app.args.port == 'COM5'

    def test_configure_logging_debug(self):
        """Test debug logging configuration"""
        app = Application()
        app.args = Mock(debug=True)
        
        with patch('PiCoMonitor.logger_setup.set_debug_level') as mock_debug:
            app.configure_logging()
            mock_debug.assert_called_once()

    def test_configure_logging_normal(self):
        """Test normal logging configuration"""
        app = Application()
        app.args = Mock(debug=False)
        
        with patch('PiCoMonitor.logger_setup.set_debug_level') as mock_debug:
            app.configure_logging()
            mock_debug.assert_not_called()

    def test_validate_arguments_success(self, valid_args):
        """Test successful argument validation"""
        app = Application()
        app.args = valid_args
        assert app.validate_arguments()

    def test_validate_arguments_failure(self, invalid_delay_args):
        """Test failed argument validation"""
        app = Application()
        app.args = invalid_delay_args
        assert not app.validate_arguments()

    def test_setup_signal_handlers(self):
        """Test signal handler setup"""
        app = Application()
        app.tray_icon = Mock()
        
        with patch('signal.signal') as mock_signal:
            app.setup_signal_handlers()
            # Should set up handlers for SIGINT and SIGTERM
            assert mock_signal.call_count == 2

    def test_initialize_components(self):
        """Test component initialization"""
        app = Application()
        app.args = Mock(delay=0.5, port='COM5')
        
        app.initialize_components()
        assert app.tray_icon is not None
        assert app.data_collector is not None

    def test_run_success(self, monkeypatch):
        """Test successful application run"""
        app = Application()
        app.args = Mock(delay=0.5, port='COM5', debug=False)
        app.tray_icon = Mock()
        app.data_collector = Mock()
        
        # Mock the run method to avoid blocking
        def mock_run(work_func):
            # Call work function once
            work_func(app.tray_icon.icon)
        
        app.tray_icon.run = mock_run
        app.data_collector.work_loop = Mock()
        
        # This should not raise any exceptions
        app.run()

    def test_run_invalid_args(self, invalid_delay_args, caplog):
        """Test application run with invalid arguments"""
        # Test the main function directly since that's where the SystemExit happens
        with patch('sys.argv', ['PiCoMonitor.py', '--delay', '0.05', '--port', 'COM5']):
            with patch('PiCoMonitor.Application') as mock_app_class:
                mock_app = Mock()
                mock_app.validate_arguments.return_value = False
                mock_app_class.return_value = mock_app
                
                with pytest.raises(SystemExit) as exc_info:
                    from PiCoMonitor import main
                    main()
                
                # Check that validation failed and SystemExit was called with code 1
                assert exc_info.value.code == 1
                assert "Argument validation failed" in caplog.text