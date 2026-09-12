#!/usr/bin/env python3
"""
Test cases for DataCollector class
"""

import pytest
import json
import serial
from unittest.mock import Mock, patch
from PiCoMonitor import DataCollector


class TestDataCollector:
    """Test DataCollector functionality"""

    def test_collect_system_data_success(self):
        """Test successful system data collection"""
        with patch('PiCoMonitor.SystemMonitor.get_cpu_usage') as mock_cpu:
            with patch('PiCoMonitor.SystemMonitor.get_cpu_temperature') as mock_temp:
                with patch('PiCoMonitor.SystemMonitor.get_memory_usage') as mock_mem:
                    with patch('PiCoMonitor.SystemMonitor.get_disk_usage') as mock_disk:
                        # Set up mock return values
                        mock_cpu.return_value = [10.5, 20.3]
                        mock_temp.return_value = 55.0
                        mock_mem.return_value = 65.5
                        mock_disk.return_value = [{'path': 'C:', 'total': 200.0, 'used': 100.0}]
                        
                        dc = DataCollector('COM5', 0.5)
                        data = dc.collect_system_data()
                        
                        assert data['CPU'] == [10.5, 20.3]
                        assert data['TEMP'] == 55.0
                        assert data['RAM'] == 65.5
                        assert data['DISKS'] == [{'path': 'C:', 'total': 200.0, 'used': 100.0}]

    def test_collect_system_data_partial_failure(self):
        """Test system data collection with some failures"""
        with patch('PiCoMonitor.SystemMonitor.get_cpu_usage') as mock_cpu:
            with patch('PiCoMonitor.SystemMonitor.get_cpu_temperature') as mock_temp:
                with patch('PiCoMonitor.SystemMonitor.get_memory_usage') as mock_mem:
                    with patch('PiCoMonitor.SystemMonitor.get_disk_usage') as mock_disk:
                        # Set up mock return values with some failures
                        mock_cpu.return_value = []  # Failure
                        mock_temp.return_value = None  # Failure
                        mock_mem.return_value = 65.5  # Success
                        mock_disk.return_value = []  # Failure
                        
                        dc = DataCollector('COM5', 0.5)
                        data = dc.collect_system_data()
                        
                        # All keys should still be present
                        assert 'CPU' in data
                        assert 'TEMP' in data
                        assert 'RAM' in data
                        assert 'DISKS' in data
                        assert data['CPU'] == []
                        assert data['TEMP'] is None
                        assert data['RAM'] == 65.5
                        assert data['DISKS'] == []

    @pytest.mark.parametrize('exception', [
        pytest.param(json.JSONDecodeError('msg', 'doc', 0), id='json_decode_error'),
        pytest.param(UnicodeEncodeError('utf-8', 'x', 0, 1, 'msg'), id='unicode_encode_error'),
        pytest.param(serial.SerialTimeoutException, id='serial_timeout'),
        pytest.param(serial.SerialException('boom'), id='serial_exception'),
    ])
    def test_send_data_via_serial_exceptions(self, exception, dummy_serial, caplog):
        """Test send data with various exceptions"""
        dc = DataCollector('COM5', 0.5)
        
        # Set up dummy serial to raise exception
        if isinstance(exception, json.JSONDecodeError):
            dummy_serial.write = Mock(side_effect=exception)
        elif isinstance(exception, UnicodeEncodeError):
            dummy_serial.write = Mock(side_effect=exception)
        elif isinstance(exception, serial.SerialTimeoutException):
            dummy_serial.write = Mock(side_effect=exception)
        else:
            dummy_serial.write = Mock(side_effect=exception)
        
        # For SerialException, we expect it to raise SerialCommunicationError
        if isinstance(exception, serial.SerialException):
            with pytest.raises(Exception):  # Catch the raised exception
                result = dc.send_data_via_serial({'test': 'data'}, dummy_serial)
        else:
            result = dc.send_data_via_serial({'test': 'data'}, dummy_serial)
            # All other exceptions should result in False return
            assert result is False
        
        # Verify appropriate log message
        if isinstance(exception, (json.JSONDecodeError, UnicodeEncodeError)):
            assert "Failed to serialize or encode data" in caplog.text
        elif isinstance(exception, serial.SerialTimeoutException):
            assert "Serial write timeout occurred" in caplog.text
        elif isinstance(exception, serial.SerialException):
            assert "Serial write failed" in caplog.text

    def test_send_data_via_serial_success(self, dummy_serial):
        """Test successful data sending"""
        dc = DataCollector('COM5', 0.5)
        result = dc.send_data_via_serial({'test': 'data'}, dummy_serial)
        assert result is True

    def test_validate_serial_port_success(self):
        """Test successful serial port validation"""
        with patch('PiCoMonitor.sys.platform', 'win32'):
            dc = DataCollector('COM5', 0.5)
            assert dc.validate_serial_port()

    def test_validate_serial_port_failure(self, caplog):
        """Test failed serial port validation"""
        with patch('PiCoMonitor.sys.platform', 'win32'):
            dc = DataCollector('INVALID', 0.5)
            assert not dc.validate_serial_port()
            assert "Invalid COM port format" in caplog.text

    def test_stop_data_collection(self):
        """Test stopping data collection"""
        dc = DataCollector('COM5', 0.5)
        dc.stop()
        assert not dc.contflag