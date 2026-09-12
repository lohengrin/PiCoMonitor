#!/usr/bin/env python3
"""
Test cases for HardwareMonitor class
"""

import pytest
from unittest.mock import Mock, patch
from PiCoMonitor import HardwareMonitor


def test_hardware_monitor_success():
    """Test successful hardware monitor initialization"""
    # Mock the Computer class
    with patch('PiCoMonitor.Computer') as mock_computer:
        mock_instance = Mock()
        mock_instance.GPUEnabled = True
        mock_instance.Open = Mock()
        mock_instance.Hardware = [Mock()]
        mock_instance.Hardware[0].Sensors = []
        mock_computer.return_value = mock_instance
        
        hm = HardwareMonitor()
        assert hm.is_available()
        assert hm.get_gpu_temperature() is None  # No sensors


def test_hardware_monitor_init_failure(caplog):
    """Test hardware monitor initialization failure"""
    with patch('PiCoMonitor.Computer') as mock_computer:
        mock_computer.side_effect = RuntimeError('Initialization failed')
        
        hm = HardwareMonitor()
        assert not hm.is_available()
        assert "Failed to initialize hardware monitoring" in caplog.text


def test_gpu_temperature_success():
    """Test successful GPU temperature reading"""
    with patch('PiCoMonitor.Computer') as mock_computer:
        # Create mock sensor
        mock_sensor = Mock()
        mock_sensor.Identifier = '/temperature/0'
        mock_sensor.get_Value = Mock(return_value=42.5)
        
        mock_hardware = Mock()
        mock_hardware.Sensors = [mock_sensor]
        mock_hardware.Update = Mock()
        
        mock_instance = Mock()
        mock_instance.GPUEnabled = True
        mock_instance.Open = Mock()
        mock_instance.Hardware = [mock_hardware]
        mock_computer.return_value = mock_instance
        
        hm = HardwareMonitor()
        assert hm.get_gpu_temperature() == 42.5


def test_gpu_temperature_no_sensor(caplog):
    """Test GPU temperature when no sensor found"""
    with patch('PiCoMonitor.Computer') as mock_computer:
        mock_hardware = Mock()
        mock_hardware.Sensors = []
        mock_hardware.Update = Mock()
        
        mock_instance = Mock()
        mock_instance.GPUEnabled = True
        mock_instance.Open = Mock()
        mock_instance.Hardware = [mock_hardware]
        mock_computer.return_value = mock_instance
        
        hm = HardwareMonitor()
        result = hm.get_gpu_temperature()
        assert result is None
        assert "No temperature sensor found" in caplog.text


def test_gpu_temperature_exception(caplog):
    """Test GPU temperature when exception occurs"""
    with patch('PiCoMonitor.Computer') as mock_computer:
        mock_hardware = Mock()
        mock_hardware.Sensors = []
        mock_hardware.Update = Mock(side_effect=Exception('Sensor error'))
        
        mock_instance = Mock()
        mock_instance.GPUEnabled = True
        mock_instance.Open = Mock()
        mock_instance.Hardware = [mock_hardware]
        mock_computer.return_value = mock_instance
        
        hm = HardwareMonitor()
        result = hm.get_gpu_temperature()
        assert result is None
        assert "Failed to get GPU temperature" in caplog.text