#!/usr/bin/env python3
"""
Additional test cases for HardwareMonitor class to improve coverage
"""

import pytest
from unittest.mock import Mock, patch
from PiCoMonitor import HardwareMonitor


class TestHardwareMonitorAdditional:
    """Additional tests for HardwareMonitor to improve coverage"""

    def test_hardware_monitor_multiple_sensors(self):
        """Test hardware monitor with multiple temperature sensors"""
        with patch('PiCoMonitor.Computer') as mock_computer:
            # Create multiple sensors
            sensor1 = Mock()
            sensor1.Identifier = '/temperature/0'
            sensor1.get_Value = Mock(return_value=42.5)
            
            sensor2 = Mock()
            sensor2.Identifier = '/temperature/1'
            sensor2.get_Value = Mock(return_value=55.0)
            
            sensor3 = Mock()
            sensor3.Identifier = '/load/0'  # Not a temperature sensor
            sensor3.get_Value = Mock(return_value=30.0)
            
            mock_hardware = Mock()
            mock_hardware.Sensors = [sensor1, sensor2, sensor3]
            mock_hardware.Update = Mock()
            
            mock_instance = Mock()
            mock_instance.GPUEnabled = True
            mock_instance.Open = Mock()
            mock_instance.Hardware = [mock_hardware]
            mock_computer.return_value = mock_instance
            
            hm = HardwareMonitor()
            # Should return the first temperature sensor found
            assert hm.get_gpu_temperature() == 42.5

    def test_hardware_monitor_no_gpu(self):
        """Test hardware monitor on system without GPU"""
        with patch('PiCoMonitor.Computer') as mock_computer:
            # No GPU sensors, only CPU
            cpu_sensor = Mock()
            cpu_sensor.Identifier = '/load/0'
            cpu_sensor.get_Value = Mock(return_value=30.0)
            
            mock_hardware = Mock()
            mock_hardware.Sensors = [cpu_sensor]
            mock_hardware.Update = Mock()
            
            mock_instance = Mock()
            mock_instance.GPUEnabled = True
            mock_instance.Open = Mock()
            mock_instance.Hardware = [mock_hardware]
            mock_computer.return_value = mock_instance
            
            hm = HardwareMonitor()
            # Should return None when no temperature sensor found
            assert hm.get_gpu_temperature() is None

    def test_hardware_monitor_permission_error(self, caplog):
        """Test hardware monitor with permission errors"""
        with patch('PiCoMonitor.Computer') as mock_computer:
            mock_instance = Mock()
            mock_instance.GPUEnabled = True
            mock_instance.Open = Mock(side_effect=PermissionError('Access denied'))
            mock_computer.return_value = mock_instance
            
            hm = HardwareMonitor()
            assert not hm.is_available()
            assert "Failed to initialize hardware monitoring" in caplog.text

    def test_hardware_monitor_multiple_hardware_types(self):
        """Test hardware monitor with multiple hardware types"""
        with patch('PiCoMonitor.Computer') as mock_computer:
            # Motherboard sensor
            mb_sensor = Mock()
            mb_sensor.Identifier = '/temperature/0'
            mb_sensor.get_Value = Mock(return_value=35.0)
            
            # GPU sensor
            gpu_sensor = Mock()
            gpu_sensor.Identifier = '/gpu/0/temperature/0'
            gpu_sensor.get_Value = Mock(return_value=65.0)
            
            mock_hardware1 = Mock()  # Motherboard
            mock_hardware1.Sensors = [mb_sensor]
            mock_hardware1.Update = Mock()
            
            mock_hardware2 = Mock()  # GPU
            mock_hardware2.Sensors = [gpu_sensor]
            mock_hardware2.Update = Mock()
            
            mock_instance = Mock()
            mock_instance.GPUEnabled = True
            mock_instance.Open = Mock()
            mock_instance.Hardware = [mock_hardware1, mock_hardware2]
            mock_computer.return_value = mock_instance
            
            hm = HardwareMonitor()
            # Should find temperature from first hardware with temperature sensor
            assert hm.get_gpu_temperature() == 35.0

    def test_hardware_monitor_sensor_reading_error(self, caplog):
        """Test hardware monitor when sensor reading fails"""
        with patch('PiCoMonitor.Computer') as mock_computer:
            sensor = Mock()
            sensor.Identifier = '/temperature/0'
            sensor.get_Value = Mock(side_effect=Exception('Sensor read error'))
            
            mock_hardware = Mock()
            mock_hardware.Sensors = [sensor]
            mock_hardware.Update = Mock()
            
            mock_instance = Mock()
            mock_instance.GPUEnabled = True
            mock_instance.Open = Mock()
            mock_instance.Hardware = [mock_hardware]
            mock_computer.return_value = mock_instance
            
            hm = HardwareMonitor()
            result = hm.get_gpu_temperature()
            assert result is None
            assert "Failed to get GPU temperature" in caplog.text

    def test_hardware_monitor_no_hardware_available(self):
        """Test hardware monitor when no hardware is available"""
        with patch('PiCoMonitor.Computer') as mock_computer:
            mock_instance = Mock()
            mock_instance.GPUEnabled = True
            mock_instance.Open = Mock()
            mock_instance.Hardware = []  # No hardware available
            mock_computer.return_value = mock_instance
            
            hm = HardwareMonitor()
            assert hm.is_available()
            assert hm.get_gpu_temperature() is None