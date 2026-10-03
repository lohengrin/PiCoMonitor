#!/usr/bin/env python3
"""
Tests for the optional extra metrics (network, disk I/O, frequency, load, swap,
uptime, GPU) and their omission from the frame when unavailable
"""

from types import SimpleNamespace
from unittest.mock import Mock, patch

import pytest

import PiCoMonitor
from PiCoMonitor import DataCollector, GpuMonitor, Metric, RateTracker, SystemMonitor


class FakeClock:
    def __init__(self):
        self.t = 100.0

    def __call__(self):
        return self.t


class TestRateTracker:
    def test_first_call_has_no_rate_then_kb_per_second(self):
        clock = FakeClock()
        rt = RateTracker(clock)
        counters = iter([SimpleNamespace(bytes_recv=0, bytes_sent=0),
                         SimpleNamespace(bytes_recv=2_000_000, bytes_sent=500_000)])
        with patch('PiCoMonitor.psutil.net_io_counters', side_effect=lambda: next(counters)):
            assert rt.network() is None
            clock.t += 2.0
            assert rt.network() == [1000.0, 250.0]

    def test_counter_reset_counts_as_zero(self):
        clock = FakeClock()
        rt = RateTracker(clock)
        counters = iter([SimpleNamespace(read_bytes=5000, write_bytes=5000),
                         SimpleNamespace(read_bytes=100, write_bytes=7000)])
        with patch('PiCoMonitor.psutil.disk_io_counters', side_effect=lambda: next(counters)):
            rt.disk_io()
            clock.t += 1.0
            assert rt.disk_io() == [0.0, 2.0]

    def test_unavailable_counters_give_none(self):
        rt = RateTracker(FakeClock())
        with patch('PiCoMonitor.psutil.net_io_counters', return_value=None):
            assert rt.network() is None
        with patch('PiCoMonitor.psutil.disk_io_counters', side_effect=RuntimeError("no perf counters")):
            assert rt.disk_io() is None


class TestSimpleMetrics:
    def test_cpu_frequency(self):
        with patch('PiCoMonitor.psutil.cpu_freq', return_value=SimpleNamespace(current=3600.4)):
            assert SystemMonitor.get_cpu_frequency() == 3600.0
        with patch('PiCoMonitor.psutil.cpu_freq', return_value=None):
            assert SystemMonitor.get_cpu_frequency() is None
        with patch('PiCoMonitor.psutil.cpu_freq', side_effect=NotImplementedError):
            assert SystemMonitor.get_cpu_frequency() is None

    def test_load_average(self):
        with patch('PiCoMonitor.psutil.getloadavg', return_value=(0.123, 1.0, 2.456)):
            assert SystemMonitor.get_load_average() == [0.12, 1.0, 2.46]
        with patch('PiCoMonitor.psutil.getloadavg', side_effect=AttributeError):
            assert SystemMonitor.get_load_average() is None

    def test_swap_and_uptime(self):
        with patch('PiCoMonitor.psutil.swap_memory', return_value=SimpleNamespace(percent=12.34)):
            assert SystemMonitor.get_swap_usage() == 12.3
        with patch('PiCoMonitor.psutil.boot_time', return_value=1000.0), patch('PiCoMonitor.time.time', return_value=1090.7):
            assert SystemMonitor.get_uptime() == 90
        with patch('PiCoMonitor.psutil.swap_memory', side_effect=OSError):
            assert SystemMonitor.get_swap_usage() is None


class TestOptionalMetrics:
    def test_none_optional_is_omitted_but_required_is_sent_as_null(self):
        dc = DataCollector('COM5', 0.5, metrics=[Metric('REQ', lambda d: None),
                                                 Metric('OPT', lambda d: None, optional=True),
                                                 Metric('OPT2', lambda d: [1], optional=True)])
        assert dc.collect_system_data() == {'REQ': None, 'OPT2': [1]}

    def test_failing_optional_metric_is_omitted(self):
        def boom(d):
            raise RuntimeError("x")
        dc = DataCollector('COM5', 0.5, metrics=[Metric('OPT', boom, optional=True), Metric('A', lambda d: 1)])
        assert dc.collect_system_data() == {'A': 1}

    def test_default_metrics_include_extras_as_optional(self):
        extras = {m.key: m.optional for m in DataCollector('COM5', 0.5).metrics}
        for key in ('NET', 'IO', 'FREQ', 'LOAD', 'SWAP', 'UP', 'GPU'):
            assert extras[key] is True
        for key in ('CPU', 'TEMP', 'RAM', 'DISKS'):
            assert extras[key] is False


class TestGpuMonitor:
    def test_no_gpu_returns_none(self):
        with patch('PiCoMonitor.pynvml', None), patch('PiCoMonitor.glob.glob', return_value=[]):
            gm = GpuMonitor()
            assert gm.read() is None
            assert gm.read() is None          # probed once

    def test_nvidia(self):
        nv = Mock()
        nv.nvmlDeviceGetCount.return_value = 1
        nv.nvmlDeviceGetName.return_value = b"Quadro P4000"
        nv.nvmlDeviceGetUtilizationRates.return_value = SimpleNamespace(gpu=37)
        nv.nvmlDeviceGetMemoryInfo.return_value = SimpleNamespace(used=2048 * 1024 * 1024, total=8192 * 1024 * 1024)
        nv.nvmlDeviceGetTemperature.return_value = 61
        nv.NVML_TEMPERATURE_GPU = 0
        with patch('PiCoMonitor.pynvml', nv):
            assert GpuMonitor().read() == {"n": "Quadro P4000", "l": 37.0, "t": 61.0, "mu": 2048, "mt": 8192}

    def test_nvml_init_failure_falls_back_to_none(self):
        nv = Mock()
        nv.nvmlInit.side_effect = RuntimeError("no driver")
        with patch('PiCoMonitor.pynvml', nv), patch('PiCoMonitor.glob.glob', return_value=[]):
            assert GpuMonitor().read() is None

    def test_amd_sysfs(self, tmp_path):
        dev = tmp_path / "card0" / "device"
        (dev / "hwmon" / "hwmon3").mkdir(parents=True)
        (dev / "gpu_busy_percent").write_text("42\n")
        (dev / "mem_info_vram_used").write_text(str(1024 * 1024 * 1024))
        (dev / "mem_info_vram_total").write_text(str(4 * 1024 * 1024 * 1024))
        (dev / "hwmon" / "hwmon3" / "temp1_input").write_text("55000\n")

        real_glob = PiCoMonitor.glob.glob

        def fake_glob(pattern):
            if pattern.startswith("/sys/class/drm"):
                return [str(dev)]
            return real_glob(pattern)

        with patch('PiCoMonitor.pynvml', None), patch('PiCoMonitor.sys.platform', 'linux'), \
                patch('PiCoMonitor.glob.glob', side_effect=fake_glob):
            out = GpuMonitor().read()
        assert out == {"n": "AMD GPU", "l": 42.0, "t": 55.0, "mu": 1024, "mt": 4096}
