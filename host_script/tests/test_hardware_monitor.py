#!/usr/bin/env python3
"""
Test cases for HardwareMonitor (LibreHardwareMonitor, Windows only)
"""

import pytest
from types import SimpleNamespace
from unittest.mock import Mock, patch
import PiCoMonitor
from PiCoMonitor import HardwareMonitor


def sensor(name, value, kind="Temperature", ident=None):
    return SimpleNamespace(Name=name, Value=value, SensorType=kind,
                           Identifier=ident or f"/hw/0/{kind.lower()}/0")


def hardware(kind, sensors, sub=()):
    return SimpleNamespace(HardwareType=kind, Sensors=list(sensors), Update=Mock(),
                           SubHardware=[hardware("Sub", s) for s in sub])


@pytest.fixture
def lhm(monkeypatch):
    """Fake LibreHardwareMonitor: returns a function building a HardwareMonitor
    over the given hardware list"""
    monkeypatch.setattr(PiCoMonitor.sys, "platform", "win32")
    monkeypatch.setattr(PiCoMonitor, "SensorType", SimpleNamespace(Temperature="Temperature"))

    def make(*hw):
        computer = Mock()
        computer.Hardware = list(hw)
        monkeypatch.setattr(PiCoMonitor, "Computer", Mock(return_value=computer))
        return HardwareMonitor()
    return make


def test_init_enables_cpu_and_gpu(lhm):
    hm = lhm()
    assert hm.is_available()
    assert hm.computer.IsCpuEnabled and hm.computer.IsGpuEnabled
    hm.computer.Open.assert_called_once()


def test_init_failure(monkeypatch, caplog):
    monkeypatch.setattr(PiCoMonitor.sys, "platform", "win32")
    monkeypatch.setattr(PiCoMonitor, "Computer", Mock(side_effect=RuntimeError("boom")))
    hm = HardwareMonitor()
    assert not hm.is_available()
    assert hm.get_temperature() is None
    assert "Failed to initialize hardware monitoring" in caplog.text


def test_open_permission_error(lhm, monkeypatch, caplog):
    computer = Mock()
    computer.Open.side_effect = PermissionError("Access denied")
    monkeypatch.setattr(PiCoMonitor, "Computer", Mock(return_value=computer))
    assert not HardwareMonitor().is_available()
    assert "Failed to initialize hardware monitoring" in caplog.text


def test_cpu_package_preferred(lhm):
    hm = lhm(hardware("Cpu", [sensor("Core #1", 50.0), sensor("CPU Package", 61.0)]),
             hardware("GpuNvidia", [sensor("GPU Core", 48.0)]))
    assert hm.get_temperature() == 61.0


def test_amd_tctl(lhm):
    hm = lhm(hardware("Cpu", [sensor("CCD1 (Tdie)", 40.0), sensor("Core (Tctl/Tdie)", 55.5)]))
    assert hm.get_temperature() == 55.5


def test_unreadable_cpu_falls_back_to_gpu_core(lhm):
    # without admin rights LHM reports the CPU sensors as 0
    hm = lhm(hardware("Cpu", [sensor("Core (Tctl/Tdie)", 0.0)]),
             hardware("GpuNvidia", [sensor("GPU Hot Spot", 60.4), sensor("GPU Core", 48.0)]))
    assert hm.get_temperature() == 48.0


def test_non_temperature_sensors_ignored(lhm):
    hm = lhm(hardware("Cpu", [sensor("CPU Total", 30.0, kind="Load")]))
    assert hm.get_temperature() is None


def test_unknown_names_take_first_reading(lhm):
    hm = lhm(hardware("GpuAmd", [sensor("GPU VR SoC", 52.0), sensor("GPU Memory", 58.0)]))
    assert hm.get_temperature() == 52.0


def test_sub_hardware_sensors(lhm):
    hm = lhm(hardware("Cpu", [], sub=[[sensor("CPU Package", 47.0)]]))
    assert hm.get_temperature() == 47.0


def test_other_hardware_ignored(lhm):
    hm = lhm(hardware("Motherboard", [sensor("System", 35.0)]))
    assert hm.get_temperature() is None


def test_null_value_ignored(lhm):
    hm = lhm(hardware("Cpu", [sensor("CPU Package", None)]),
             hardware("GpuNvidia", [sensor("GPU Core", 45.0)]))
    assert hm.get_temperature() == 45.0


def test_no_sensor(lhm, caplog):
    hm = lhm(hardware("Cpu", []))
    assert hm.get_temperature() is None
    assert "No temperature sensor found" in caplog.text


def test_no_hardware(lhm):
    hm = lhm()
    assert hm.is_available()
    assert hm.get_temperature() is None


def test_update_exception(lhm, caplog):
    hw = hardware("Cpu", [sensor("CPU Package", 50.0)])
    hw.Update.side_effect = Exception("Sensor error")
    hm = lhm(hw)
    assert hm.get_temperature() is None
    assert "Failed to get temperature" in caplog.text


def test_close(lhm):
    hm = lhm()
    computer = hm.computer
    hm.close()
    computer.Close.assert_called_once()
    assert not hm.is_available()
    hm.close()  # idempotent


def test_not_windows(monkeypatch):
    monkeypatch.setattr(PiCoMonitor.sys, "platform", "linux")
    hm = HardwareMonitor()
    assert not hm.is_available()
    assert hm.get_temperature() is None
