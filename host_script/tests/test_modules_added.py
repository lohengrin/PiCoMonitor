#!/usr/bin/env python3
"""
Tests for the modular metrics collector, Linux CPU temperature lookup,
OpenHardwareMonitor DLL lookup and headless operation
"""

import os
from types import SimpleNamespace
from unittest.mock import Mock, patch

import pytest

import PiCoMonitor
from PiCoMonitor import Application, DataCollector, Metric, SystemMonitor, find_ohm_dll


class TestMetrics:
    def test_one_failing_metric_does_not_affect_others(self):
        def boom(dc):
            raise RuntimeError("sensor exploded")

        dc = DataCollector('COM5', 0.5, metrics=[Metric('A', lambda d: 1), Metric('B', boom, fallback=-1),
                                                 Metric('C', lambda d: [2])])
        assert dc.collect_system_data() == {'A': 1, 'B': -1, 'C': [2]}

    def test_default_metrics_keys(self):
        assert [m.key for m in DataCollector('COM5', 0.5).metrics] == ['CPU', 'TEMP', 'RAM', 'DISKS']

    def test_metric_gets_collector_delay(self):
        seen = []
        dc = DataCollector('COM5', 0.25, metrics=[Metric('X', lambda d: seen.append(d.delay))])
        dc.collect_system_data()
        assert seen == [0.25]


def sensors(**kw):
    return patch('PiCoMonitor.psutil.sensors_temperatures', create=True, return_value=kw)


class TestLinuxCpuTemp:
    def test_intel_package_label_preferred(self):
        with sensors(coretemp=[('Core 0', 40.0, 0, 0), ('Package id 0', 55.0, 0, 0), ('Core 1', 41.0, 0, 0)]):
            assert SystemMonitor._get_linux_cpu_temp() == 55.0

    def test_amd_k10temp_tctl(self):
        with sensors(k10temp=[('Tccd1', 50.0, 0, 0), ('Tctl', 61.5, 0, 0)]):
            assert SystemMonitor._get_linux_cpu_temp() == 61.5

    def test_raspberry_pi_cpu_thermal(self):
        with sensors(cpu_thermal=[('', 47.2, 0, 0)]):
            assert SystemMonitor._get_linux_cpu_temp() == 47.2

    def test_cpu_sensor_beats_gpu_and_nvme(self):
        with sensors(nvme=[('Composite', 33.0, 0, 0)], amdgpu=[('edge', 70.0, 0, 0)],
                     coretemp=[('Package id 0', 52.0, 0, 0)]):
            assert SystemMonitor._get_linux_cpu_temp() == 52.0

    def test_gpu_used_when_no_cpu_sensor(self):
        with sensors(nvme=[('Composite', 33.0, 0, 0)], amdgpu=[('edge', 70.0, 0, 0)]):
            assert SystemMonitor._get_linux_cpu_temp() == 70.0

    def test_storage_and_wifi_never_reported(self):
        with sensors(nvme=[('Composite', 33.0, 0, 0)], iwlwifi_1=[('', 44.0, 0, 0)]):
            assert SystemMonitor._get_linux_cpu_temp() is None

    def test_no_sensors_and_none_readings(self):
        with sensors():
            assert SystemMonitor._get_linux_cpu_temp() is None
        with sensors(coretemp=[('Package id 0', None, 0, 0)]):
            assert SystemMonitor._get_linux_cpu_temp() is None

    def test_psutil_without_sensor_support(self):
        with patch('PiCoMonitor.psutil.sensors_temperatures', side_effect=AttributeError, create=True):
            assert SystemMonitor._get_linux_cpu_temp() is None


class TestOhmLookup:
    def test_env_var_wins(self, tmp_path, monkeypatch):
        dll = tmp_path / "x.dll"
        dll.write_text("")
        monkeypatch.setenv("PICOMONITOR_OHM_DLL", str(dll))
        assert find_ohm_dll() == str(dll)

    def test_not_found(self, monkeypatch):
        monkeypatch.delenv("PICOMONITOR_OHM_DLL", raising=False)
        with patch('PiCoMonitor.os.path.isfile', return_value=False):
            assert find_ohm_dll() is None

    def test_next_to_script(self, monkeypatch):
        monkeypatch.delenv("PICOMONITOR_OHM_DLL", raising=False)
        wanted = os.path.join(os.path.dirname(os.path.abspath(PiCoMonitor.__file__)),
                              "OpenHardwareMonitor", "OpenHardwareMonitorLib.dll")
        with patch('PiCoMonitor.os.path.isfile', side_effect=lambda p: p == wanted):
            assert find_ohm_dll() == wanted


class TestHeadless:
    def test_no_tray_flag_parsed(self):
        app = Application()
        with patch('sys.argv', ['PiCoMonitor.py', '--no-tray']):
            app.parse_arguments()
        assert app.args.no_tray and app.args.port is None and not app.args.list_ports

    def test_no_tray_skips_tray_creation(self):
        app = Application()
        app.args = SimpleNamespace(delay=0.5, port=None, no_tray=True)
        with patch('PiCoMonitor.SystemTrayIcon') as tray:
            app.initialize_components()
        tray.assert_not_called()
        assert app.tray_icon is None and app.data_collector is not None

    def test_tray_failure_falls_back_to_headless(self):
        app = Application()
        app.args = SimpleNamespace(delay=0.5, port=None, no_tray=False)
        with patch('PiCoMonitor.SystemTrayIcon', side_effect=RuntimeError("no display")):
            app.initialize_components()
        assert app.tray_icon is None and app.data_collector is not None

    def test_headless_run_calls_work_loop(self):
        app = Application()
        app.args = SimpleNamespace(delay=0.5, port=None, no_tray=True)
        app.tray_icon = None
        app.data_collector = Mock()
        app.run()
        app.data_collector.work_loop.assert_called_once()


class TestDiskLabel:
    def test_windows_keeps_colon(self):
        with patch('PiCoMonitor.sys.platform', 'win32'):
            assert SystemMonitor._get_disk_label('C:\\') == 'C:'
            assert SystemMonitor._get_disk_label('d:\\') == 'D:'

    def test_windows_non_drive_path_unchanged(self):
        with patch('PiCoMonitor.sys.platform', 'win32'):
            assert SystemMonitor._get_disk_label('\\\\server\\share') == '\\\\server\\share'

    def test_linux_labels(self):
        with patch('PiCoMonitor.sys.platform', 'linux'):
            assert SystemMonitor._get_disk_label('/') == '/'
            assert SystemMonitor._get_disk_label('/mnt/Data') == 'Data'
