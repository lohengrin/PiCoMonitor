#!/usr/bin/env python3
"""
Additional test cases for SerialPortManager class to improve coverage
"""

import pytest
import serial
from unittest.mock import Mock, patch
from PiCoMonitor import SerialPortManager, SerialCommunicationError


class TestSerialPortManagerAdditional:
    """Additional tests for SerialPortManager to improve coverage"""

    def test_serial_port_already_open(self, dummy_serial):
        """Test serial port already in use"""
        with patch('PiCoMonitor.serial.Serial') as mock_serial:
            mock_serial.side_effect = serial.SerialException('Port already open')
            
            with pytest.raises(SerialCommunicationError):
                with SerialPortManager('COM5'):
                    pass

    def test_serial_port_permission_denied(self, dummy_serial):
        """Test serial port permission errors"""
        with patch('PiCoMonitor.serial.Serial') as mock_serial:
            mock_serial.side_effect = PermissionError('Access denied')
            
            with pytest.raises(Exception):
                with SerialPortManager('COM5'):
                    pass

    def test_serial_port_multiple_reconnects(self, dummy_serial):
        """Test multiple reconnection attempts"""
        attempt_count = 0
        
        def mock_serial_with_retry(*args, **kwargs):
            nonlocal attempt_count
            attempt_count += 1
            if attempt_count < 3:
                raise serial.SerialException(f'Attempt {attempt_count} failed')
            return dummy_serial
        
        with patch('PiCoMonitor.serial.Serial') as mock_serial:
            mock_serial.side_effect = mock_serial_with_retry
            
            # Should succeed on third attempt
            with SerialPortManager('COM5') as conn:
                assert conn is dummy_serial
                assert attempt_count == 3

    def test_serial_port_different_configurations(self):
        """Test different serial port configurations"""
        test_configs = [
            {'port': 'COM3', 'baudrate': 9600, 'timeout': 0.5},
            {'port': 'COM10', 'baudrate': 115200, 'timeout': 2},
            {'port': '/dev/ttyUSB0', 'baudrate': 57600, 'timeout': 1},
        ]
        
        for config in test_configs:
            with patch('PiCoMonitor.serial.Serial') as mock_serial:
                mock_conn = Mock()
                mock_conn.name = config['port']
                mock_conn.is_open = True
                mock_conn.closed = False
                mock_serial.return_value = mock_conn
                
                with SerialPortManager(
                    config['port'], 
                    config['baudrate'], 
                    config['timeout']
                ) as conn:
                    assert conn.name == config['port']
                
                # Verify port was closed
                assert mock_conn.closed

    def test_serial_port_context_manager_exception_handling(self, dummy_serial):
        """Test exception handling within context manager"""
        with patch('PiCoMonitor.serial.Serial') as mock_serial:
            mock_serial.return_value = dummy_serial
            
            # Exception should not prevent port cleanup
            with pytest.raises(ValueError):
                with SerialPortManager('COM5') as conn:
                    raise ValueError('Test exception during operation')
            
            # Port should still be closed even with exception
            assert dummy_serial.closed

    def test_serial_port_invalid_baud_rate(self):
        """Test invalid baud rate handling"""
        with patch('PiCoMonitor.serial.Serial') as mock_serial:
            # Some serial implementations might reject invalid baud rates
            mock_serial.side_effect = ValueError('Invalid baud rate')
            
            with pytest.raises(Exception):
                with SerialPortManager('COM5', baudrate=123456):
                    pass

    def test_serial_port_timeout_scenarios(self, dummy_serial):
        """Test different timeout scenarios"""
        # Test very short timeout
        with patch('PiCoMonitor.serial.Serial') as mock_serial:
            mock_serial.return_value = dummy_serial
            
            with SerialPortManager('COM5', timeout=0.1) as conn:
                assert conn.timeout == 0.1
            
            # Test longer timeout
            with SerialPortManager('COM5', timeout=5) as conn:
                assert conn.timeout == 5

    def test_serial_port_multiple_instances(self, dummy_serial):
        """Test multiple serial port instances"""
        with patch('PiCoMonitor.serial.Serial') as mock_serial:
            mock_serial.return_value = dummy_serial
            
            # Create multiple managers
            with SerialPortManager('COM5') as conn1:
                with SerialPortManager('COM6') as conn2:
                    # Both should be active
                    assert conn1.is_open
                    assert conn2.is_open
                
                # First should still be open
                assert conn1.is_open
            
            # Both should be closed now
            assert dummy_serial.closed