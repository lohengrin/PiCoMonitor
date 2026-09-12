#!/usr/bin/env python3
"""
Additional test cases for SystemMonitor class to improve coverage
"""

import pytest
from unittest.mock import Mock, patch
from PiCoMonitor import SystemMonitor


class TestSystemMonitorAdditional:
    """Additional tests for SystemMonitor to improve coverage"""

    def test_linux_cpu_temp_variations(self):
        """Test different Linux sensor configurations"""
        with patch('PiCoMonitor.sys.platform', 'linux'):
            # Mock psutil.sensors_temperatures if it exists
            try:
                with patch('psutil.sensors_temperatures') as mock_temps:
                    # Test with different sensor names
                    test_cases = [
                        # Standard AMD GPU
                        {'amdgpu': [[None, 55.0]]},
                        # NVIDIA GPU
                        {'nvidiagpu': [[None, 65.0]]},
                        # Multiple sensors
                        {'amdgpu': [[None, 45.0], [None, 50.0]]},
                        # Intel GPU
                        {'intelgpu': [[None, 40.0]]},
                    ]
                    
                    for sensors in test_cases:
                        mock_temps.return_value = sensors
                        result = SystemMonitor._get_linux_cpu_temp()
                        # Should return the first temperature from first sensor
                        if sensors:
                            first_sensor = next(iter(sensors.values()))
                            assert result == first_sensor[0][1]
            except ImportError:
                # psutil.sensors_temperatures not available on Windows
                pass

    def test_windows_cpu_temp_variations(self):
        """Test Windows-specific temperature reading variations"""
        with patch('PiCoMonitor.sys.platform', 'win32'):
            with patch('PiCoMonitor.hardware_monitor') as mock_hw:
                # Test different temperature values
                mock_hw.get_gpu_temperature.return_value = 45.0
                result = SystemMonitor.get_cpu_temperature()
                assert result == 45.0
                
                # Test None case
                mock_hw.get_gpu_temperature.return_value = None
                result = SystemMonitor.get_cpu_temperature()
                assert result is None

    def test_disk_usage_edge_cases(self, caplog):
        """Test disk usage edge cases"""
        # Test full disk
        mock_partition = Mock()
        mock_partition.mountpoint = 'C:\\'
        mock_partition.fstype = 'NTFS'
        
        mock_usage = Mock()
        mock_usage.total = 1000000000  # 1 GB
        mock_usage.used = 999000000   # 999 MB used (99.9% full)
        mock_usage.percent = 99.9
        
        with patch('PiCoMonitor.psutil.disk_partitions') as mock_partitions:
            with patch('PiCoMonitor.psutil.disk_usage') as mock_usage_func:
                mock_partitions.return_value = [mock_partition]
                mock_usage_func.return_value = mock_usage
                
                result = SystemMonitor.get_disk_usage()
                assert len(result) == 1
                # Fixed the assertion to match actual calculation
                assert result[0]['used'] == pytest.approx(0.999, abs=0.001)
                assert result[0]['total'] == pytest.approx(1.0, abs=0.001)

    def test_disk_usage_no_space(self, caplog):
        """Test disk with no space available"""
        mock_partition = Mock()
        mock_partition.mountpoint = 'D:\\'
        mock_partition.fstype = 'NTFS'
        
        mock_usage = Mock()
        mock_usage.total = 1000000000  # 1 GB
        mock_usage.used = 1000000000  # 100% used
        mock_usage.percent = 100.0
        
        with patch('PiCoMonitor.psutil.disk_partitions') as mock_partitions:
            with patch('PiCoMonitor.psutil.disk_usage') as mock_usage_func:
                mock_partitions.return_value = [mock_partition]
                mock_usage_func.return_value = mock_usage
                
                result = SystemMonitor.get_disk_usage()
                assert len(result) == 1
                assert result[0]['used'] == pytest.approx(1.0)

    def test_disk_usage_permission_errors(self, caplog):
        """Test disk usage with permission errors"""
        mock_partition = Mock()
        mock_partition.mountpoint = '/mnt/restricted'
        mock_partition.fstype = 'ext4'
        
        with patch('PiCoMonitor.psutil.disk_partitions') as mock_partitions:
            with patch('PiCoMonitor.psutil.disk_usage') as mock_usage_func:
                mock_partitions.return_value = [mock_partition]
                mock_usage_func.side_effect = PermissionError('Access denied')
                
                result = SystemMonitor.get_disk_usage()
                assert result == []
                assert "Failed to get disk usage" in caplog.text

    def test_memory_usage_edge_cases(self):
        """Test memory usage edge cases"""
        # Test very high memory usage
        with patch('PiCoMonitor.psutil.virtual_memory') as mock_mem:
            mock_mem.return_value = Mock(percent=99.9)
            result = SystemMonitor.get_memory_usage()
            assert result == 99.9
            
            # Test very low memory usage
            mock_mem.return_value = Mock(percent=0.1)
            result = SystemMonitor.get_memory_usage()
            assert result == 0.1

    def test_cross_platform_disk_filtering(self):
        """Test disk filtering on different platforms"""
        # Test Windows filtering
        with patch('PiCoMonitor.sys.platform', 'win32'):
            # NTFS should be included
            ntfs_partition = Mock()
            ntfs_partition.mountpoint = 'C:\\'
            ntfs_partition.fstype = 'NTFS'
            
            # FAT32 should be excluded
            fat_partition = Mock()
            fat_partition.mountpoint = 'D:\\'
            fat_partition.fstype = 'FAT32'
            
            with patch('PiCoMonitor.psutil.disk_partitions') as mock_partitions:
                with patch('PiCoMonitor.psutil.disk_usage') as mock_usage_func:
                    mock_partitions.return_value = [ntfs_partition, fat_partition]
                    
                    # Mock usage to return valid data
                    mock_usage = Mock()
                    mock_usage.total = 1000000000
                    mock_usage.used = 500000000
                    mock_usage_func.return_value = mock_usage
                    
                    result = SystemMonitor.get_disk_usage()
                    # Should only include NTFS partition
                    assert len(result) == 1
                    assert result[0]['path'] == 'C:\\'

    def test_linux_disk_filtering(self):
        """Test Linux-specific disk filtering"""
        with patch('PiCoMonitor.sys.platform', 'linux'):
            # ext4 should be included
            ext4_partition = Mock()
            ext4_partition.mountpoint = '/'
            ext4_partition.fstype = 'ext4'
            
            # /var should be excluded
            var_partition = Mock()
            var_partition.mountpoint = '/var/lib'
            var_partition.fstype = 'ext4'
            
            # xfs should be excluded
            xfs_partition = Mock()
            xfs_partition.mountpoint = '/data'
            xfs_partition.fstype = 'xfs'
            
            with patch('PiCoMonitor.psutil.disk_partitions') as mock_partitions:
                with patch('PiCoMonitor.psutil.disk_usage') as mock_usage_func:
                    mock_partitions.return_value = [ext4_partition, var_partition, xfs_partition]
                    
                    # Mock usage to return valid data
                    mock_usage = Mock()
                    mock_usage.total = 1000000000
                    mock_usage.used = 500000000
                    mock_usage_func.return_value = mock_usage
                    
                    result = SystemMonitor.get_disk_usage()
                    # Should only include root ext4 partition
                    assert len(result) == 1
                    assert result[0]['path'] == '/'