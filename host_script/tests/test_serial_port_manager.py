#!/usr/bin/env python3
"""
Test cases for SerialPortManager class
"""

import pytest
import serial
from unittest.mock import Mock, patch
from PiCoMonitor import SerialPortManager, SerialCommunicationError


class TestSerialPortManager:
    """Test SerialPortManager functionality"""

    def test_serial_port_manager_success(self, dummy_serial):
        """Test successful serial port connection"""
        with patch('PiCoMonitor.serial.Serial') as mock_serial:
            mock_serial.return_value = dummy_serial
            
            with SerialPortManager('COM5') as conn:
                assert conn is dummy_serial
                assert dummy_serial.is_open
            
            # Verify port was closed
            assert dummy_serial.closed

    def test_serial_port_manager_failure(self, caplog):
        """Test serial port connection failure"""
        with patch('PiCoMonitor.serial.Serial') as mock_serial:
            mock_serial.side_effect = serial.SerialException('Cannot open port')
            
            with pytest.raises(SerialCommunicationError):
                with SerialPortManager('COM5'):
                    pass
            
            assert "Failed to open serial port" in caplog.text

    def test_serial_port_manager_context_exit(self, dummy_serial):
        """Test that context manager properly closes port on exit"""
        with patch('PiCoMonitor.serial.Serial') as mock_serial:
            mock_serial.return_value = dummy_serial
            
            with SerialPortManager('COM5') as conn:
                assert conn is dummy_serial
            
            # After context exit, port should be closed
            assert dummy_serial.closed
            assert not dummy_serial.is_open

    def test_serial_port_manager_exception_in_context(self, dummy_serial, caplog):
        """Test exception handling within context manager"""
        with patch('PiCoMonitor.serial.Serial') as mock_serial:
            mock_serial.return_value = dummy_serial
            
            with pytest.raises(ValueError):
                with SerialPortManager('COM5') as conn:
                    raise ValueError('Test exception')
            
            # Port should still be closed even with exception
            assert dummy_serial.closed