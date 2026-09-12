#!/usr/bin/env python3
"""
Test cases for ArgumentValidator class
"""

import pytest
from unittest.mock import Mock, patch
from PiCoMonitor import ArgumentValidator


class TestArgumentValidator:
    """Test ArgumentValidator functionality"""

    def test_validate_delay_success(self):
        """Test successful delay validation"""
        args = Mock(delay=0.5, port='COM5')
        assert ArgumentValidator.validate_delay(args.delay)

    def test_validate_delay_failure(self):
        """Test failed delay validation"""
        args = Mock(delay=0.05, port='COM5')
        with pytest.raises(ValueError):
            ArgumentValidator.validate_delay(args.delay)

    def test_validate_serial_port_success_windows(self):
        """Test successful serial port validation on Windows"""
        with patch('PiCoMonitor.sys.platform', 'win32'):
            args = Mock(delay=0.5, port='COM5')
            assert ArgumentValidator.validate_serial_port(args.port)

    def test_validate_serial_port_success_linux(self):
        """Test successful serial port validation on Linux"""
        with patch('PiCoMonitor.sys.platform', 'linux'):
            args = Mock(delay=0.5, port='/dev/ttyACM0')
            assert ArgumentValidator.validate_serial_port(args.port)

    def test_validate_serial_port_failure_windows(self):
        """Test failed serial port validation on Windows"""
        with patch('PiCoMonitor.sys.platform', 'win32'):
            args = Mock(delay=0.5, port='INVALID')
            with pytest.raises(ValueError):
                ArgumentValidator.validate_serial_port(args.port)

    def test_validate_serial_port_failure_linux(self):
        """Test failed serial port validation on Linux"""
        with patch('PiCoMonitor.sys.platform', 'linux'):
            args = Mock(delay=0.5, port='/invalid/path')
            with pytest.raises(ValueError):
                ArgumentValidator.validate_serial_port(args.port)

    def test_validate_required_modules_success(self):
        """Test successful required modules validation"""
        assert ArgumentValidator.validate_required_modules()

    def test_validate_required_modules_failure(self, caplog):
        """Test failed required modules validation"""
        # Mock the import to fail within the method
        with patch('PiCoMonitor.ArgumentValidator.validate_required_modules') as mock_method:
            mock_method.side_effect = ImportError('Missing required module: psutil not found')
            
            # Call the method and expect it to raise ImportError
            with pytest.raises(ImportError) as exc_info:
                ArgumentValidator.validate_required_modules()
            
            # Verify the exception message
            assert "Missing required module" in str(exc_info.value)

    def test_validate_all_success(self, valid_args):
        """Test successful validation of all arguments"""
        assert ArgumentValidator.validate_all(valid_args)

    def test_validate_all_failure_delay(self, invalid_delay_args, caplog):
        """Test failed validation due to invalid delay"""
        assert not ArgumentValidator.validate_all(invalid_delay_args)
        assert "Argument validation failed" in caplog.text

    def test_validate_all_failure_port(self, invalid_port_args, caplog):
        """Test failed validation due to invalid port"""
        assert not ArgumentValidator.validate_all(invalid_port_args)
        assert "Argument validation failed" in caplog.text