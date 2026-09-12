#!/usr/bin/env python3
"""
Test cases for SystemMonitor class
"""

import pytest
from unittest.mock import Mock, patch
from PiCoMonitor import SystemMonitor


class TestSystemMonitor:
    """Test SystemMonitor functionality"""

    @pytest.mark.parametrize('exception', [
        pytest.param(Exception('psutil error'), id='generic_exception'),
        pytest.param(ValueError('invalid value'), id='value_error'),
    ])
    def test_cpu_usage_failure(self, exception, caplog):
        """Test CPU usage with various exceptions"""
        with patch('PiCoMonitor.psutil.cpu_percent') as mock_cpu:
            mock_cpu.side_effect = exception
            result = SystemMonitor.get_cpu_usage(0.5)
            assert result == []
            assert "Failed to get CPU usage" in caplog.text

    def test_cpu_usage_success(self):
        """Test successful CPU usage reading"""
        with patch('PiCoMonitor.psutil.cpu_percent') as mock_cpu:
            mock_cpu.return_value = [10.5, 20.3, 15.2]
            result = SystemMonitor.get_cpu_usage(0.5)
            assert result == [10.5, 20.3, 15.2]

    def test_memory_usage_success(self):
        """Test successful memory usage reading"""
        with patch('PiCoMonitor.psutil.virtual_memory') as mock_mem:
            mock_mem.return_value = Mock(percent=65.5)
            result = SystemMonitor.get_memory_usage()
            assert result == 65.5

    def test_memory_usage_failure(self, caplog):
        """Test memory usage with exception"""
        with patch('PiCoMonitor.psutil.virtual_memory') as mock_mem:
            mock_mem.side_effect = MemoryError('Memory error')
            result = SystemMonitor.get_memory_usage()
            assert result is None
            assert "Failed to get memory usage" in caplog.text

    def test_disk_usage_success(self):
        """Test successful disk usage reading"""
        # Mock partitions and usage
        mock_partition = Mock()
        mock_partition.mountpoint = 'C:\\'
        mock_partition.fstype = 'NTFS'
        
        mock_usage = Mock()
        mock_usage.total = 200000000000  # 200 GB
        mock_usage.used = 100000000000   # 100 GB
        
        with patch('PiCoMonitor.psutil.disk_partitions') as mock_partitions:
            with patch('PiCoMonitor.psutil.disk_usage') as mock_usage_func:
                mock_partitions.return_value = [mock_partition]
                mock_usage_func.return_value = mock_usage
                
                result = SystemMonitor.get_disk_usage()
                assert len(result) == 1
                assert result[0]['path'] == 'C:\\'
                assert result[0]['total'] == pytest.approx(200.0)
                assert result[0]['used'] == pytest.approx(100.0)

    def test_disk_usage_partial_failure(self, caplog):
        """Test disk usage with some partitions failing"""
        # Good partition
        good_partition = Mock()
        good_partition.mountpoint = 'C:\\'
        good_partition.fstype = 'NTFS'
        
        # Bad partition
        bad_partition = Mock()
        bad_partition.mountpoint = 'D:\\'
        bad_partition.fstype = 'NTFS'
        
        good_usage = Mock()
        good_usage.total = 200000000000
        good_usage.used = 100000000000
        
        with patch('PiCoMonitor.psutil.disk_partitions') as mock_partitions:
            with patch('PiCoMonitor.psutil.disk_usage') as mock_usage_func:
                mock_partitions.return_value = [good_partition, bad_partition]
                
                def usage_side_effect(path):
                    if path == 'C:\\':
                        return good_usage
                    else:
                        raise OSError('Access denied')
                
                mock_usage_func.side_effect = usage_side_effect
                
                result = SystemMonitor.get_disk_usage()
                assert len(result) == 1  # Only good partition included
                assert "Failed to get disk usage" in caplog.text

    def test_linux_cpu_temp_missing(self):
        """Test Linux CPU temperature when no sensor found"""
        with patch('PiCoMonitor.sys.platform', 'linux'):
            with patch('PiCoMonitor.psutil.sensors_temperatures') as mock_temps:
                mock_temps.return_value = {}
                result = SystemMonitor._get_linux_cpu_temp()
                assert result is None

    def test_linux_cpu_temp_success(self):
        """Test successful Linux CPU temperature reading"""
        with patch('PiCoMonitor.sys.platform', 'linux'):
            with patch('PiCoMonitor.psutil.sensors_temperatures') as mock_temps:
                mock_temps.return_value = {
                    'amdgpu': [[None, 55.0]]
                }
                result = SystemMonitor._get_linux_cpu_temp()
                assert result == 55.0

    def test_linux_cpu_temp_exception(self, caplog):
        """Test Linux CPU temperature with exception"""
        with patch('PiCoMonitor.sys.platform', 'linux'):
            with patch('PiCoMonitor.psutil.sensors_temperatures') as mock_temps:
                mock_temps.side_effect = KeyError('No amdgpu sensor')
                result = SystemMonitor._get_linux_cpu_temp()
                assert result is None
                assert "Failed to get Linux CPU temperature" in caplog.text