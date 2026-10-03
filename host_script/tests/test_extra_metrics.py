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


def fake_nvml(devices):
    """Mock of pynvml with one entry per device: (name, load, vram_used_mib, vram_total_mib, temp)"""
    nv = Mock()
    nv.nvmlDeviceGetCount.return_value = len(devices)
    nv.nvmlDeviceGetHandleByIndex.side_effect = lambda i: i
    nv.nvmlDeviceGetName.side_effect = lambda h: devices[h][0].encode()
    nv.nvmlDeviceGetUtilizationRates.side_effect = lambda h: SimpleNamespace(gpu=devices[h][1])
    nv.nvmlDeviceGetMemoryInfo.side_effect = lambda h: SimpleNamespace(used=devices[h][2] * 1024 * 1024,
                                                                       total=devices[h][3] * 1024 * 1024)
    nv.nvmlDeviceGetTemperature.side_effect = lambda h, kind: devices[h][4]
    nv.NVML_TEMPERATURE_GPU = 0
    return nv


def fake_amd_card(tmp_path, index, load, used_mib, total_mib, temp_mdeg, vendor="0x1002", device="0x1638"):
    dev = tmp_path / f"card{index}" / "device"
    (dev / "hwmon" / "hwmon3").mkdir(parents=True)
    (dev / "gpu_busy_percent").write_text(f"{load}\n")
    (dev / "mem_info_vram_used").write_text(str(used_mib * 1024 * 1024))
    (dev / "mem_info_vram_total").write_text(str(total_mib * 1024 * 1024))
    (dev / "hwmon" / "hwmon3" / "temp1_input").write_text(f"{temp_mdeg}\n")
    (dev / "vendor").write_text(vendor + "\n")
    (dev / "device").write_text(device + "\n")
    return dev


PCI_IDS = """# comment
1002  Advanced Micro Devices, Inc. [AMD/ATI]
\t1638  Cezanne [Radeon Vega Series / Radeon Vega Mobile Series]
\t\t1043 8877  Some subsystem
\t73bf  Navi 21 [Radeon RX 6800/6800 XT / 6900 XT]
10de  NVIDIA Corporation
\t1bb1  GP104GL [Quadro P4000]
"""


class TestGpuNames:
    def test_pci_lookup_and_short_names(self, tmp_path):
        ids = tmp_path / "pci.ids"
        ids.write_text(PCI_IDS)
        assert PiCoMonitor.pci_device_name(0x1002, 0x1638, (str(ids),)) == \
            "Cezanne [Radeon Vega Series / Radeon Vega Mobile Series]"
        assert PiCoMonitor.pci_device_name(0x10de, 0x1bb1, (str(ids),)) == "GP104GL [Quadro P4000]"
        assert PiCoMonitor.pci_device_name(0x1002, 0xdead, (str(ids),)) is None     # unknown device
        assert PiCoMonitor.pci_device_name(0x8086, 0x1234, (str(ids),)) is None     # unknown vendor
        assert PiCoMonitor.pci_device_name(0x1002, 0x1638, ("/nonexistent/pci.ids",)) is None

    def test_short_names(self):
        f = PiCoMonitor._short_gpu_name
        assert f("Cezanne [Radeon Vega Series / Radeon Vega Mobile Series]") == "Radeon Vega Series"
        assert f("Navi 21 [Radeon RX 6800/6800 XT / 6900 XT]") == "Radeon RX 6800/6800 XT"
        assert f("Some Plain Name") == "Some Plain Name"
        assert len(f("X" * 60)) == 24


class TestGpuMonitor:
    def test_no_gpu_returns_none(self):
        with patch('PiCoMonitor.pynvml', None), patch('PiCoMonitor.glob.glob', return_value=[]):
            gm = GpuMonitor()
            assert gm.read() is None
            assert gm.read() is None          # probed once

    def test_nvidia_only(self):
        nv = fake_nvml([("Quadro P4000", 37, 2048, 8192, 61)])
        with patch('PiCoMonitor.pynvml', nv), patch('PiCoMonitor.glob.glob', return_value=[]):
            assert GpuMonitor().read() == [{"n": "Quadro P4000", "l": 37.0, "t": 61.0, "mu": 2048, "mt": 8192}]

    def test_nvml_init_failure_falls_back_to_none(self):
        nv = Mock()
        nv.nvmlInit.side_effect = RuntimeError("no driver")
        with patch('PiCoMonitor.pynvml', nv), patch('PiCoMonitor.glob.glob', return_value=[]):
            assert GpuMonitor().read() is None

    def _amd_patches(self, tmp_path, dev):
        real_glob = PiCoMonitor.glob.glob
        ids = tmp_path / "pci.ids"
        ids.write_text(PCI_IDS)

        def fake_glob(pattern):
            if pattern.startswith("/sys/class/drm"):
                return [str(d) for d in sorted(dev)]
            return real_glob(pattern)
        return (patch('PiCoMonitor.sys.platform', 'linux'), patch('PiCoMonitor.glob.glob', side_effect=fake_glob),
                patch('PiCoMonitor._PCI_IDS_PATHS', (str(ids),)))

    def test_amd_sysfs_with_friendly_name(self, tmp_path):
        dev = fake_amd_card(tmp_path, 2, 42, 1024, 4096, 55000)
        p1, p2, p3 = self._amd_patches(tmp_path, [dev])
        with patch('PiCoMonitor.pynvml', None), p1, p2, p3:
            assert GpuMonitor().read() == [{"n": "Radeon Vega Series", "l": 42.0, "t": 55.0, "mu": 1024, "mt": 4096}]

    def test_unknown_amd_device_gets_generic_name(self, tmp_path):
        dev = fake_amd_card(tmp_path, 0, 5, 100, 512, 40000, device="0x9999")
        p1, p2, p3 = self._amd_patches(tmp_path, [dev])
        with patch('PiCoMonitor.pynvml', None), p1, p2, p3:
            assert GpuMonitor().read()[0]["n"] == "AMD GPU"

    def test_nvidia_dgpu_and_amd_igpu_both_reported(self, tmp_path):
        dev = fake_amd_card(tmp_path, 2, 6, 1019, 4096, 37000)
        nv = fake_nvml([("Quadro P4000", 0, 6717, 8192, 38)])
        p1, p2, p3 = self._amd_patches(tmp_path, [dev])
        with patch('PiCoMonitor.pynvml', nv), p1, p2, p3:
            gpus = GpuMonitor().read()
        assert [g["n"] for g in gpus] == ["Quadro P4000", "Radeon Vega Series"]    # NVIDIA first
        assert gpus[1]["mu"] == 1019 and gpus[1]["t"] == 37.0

    def test_connector_entries_are_not_gpus(self, tmp_path):
        card = fake_amd_card(tmp_path, 1, 3, 10, 100, 30000)
        conn = tmp_path / "card1-DP-1" / "device"
        conn.mkdir(parents=True)
        (conn / "gpu_busy_percent").write_text("0\n")          # even if it looked like a GPU
        p1, p2, p3 = self._amd_patches(tmp_path, [card, conn])
        with patch('PiCoMonitor.pynvml', None), p1, p2, p3:
            assert len(GpuMonitor().read()) == 1

    def test_one_failing_gpu_does_not_hide_the_other(self, tmp_path):
        dev = fake_amd_card(tmp_path, 2, 6, 1019, 4096, 37000)
        nv = fake_nvml([("Quadro P4000", 0, 6717, 8192, 38)])
        nv.nvmlDeviceGetTemperature.side_effect = RuntimeError("GPU fell off the bus")
        p1, p2, p3 = self._amd_patches(tmp_path, [dev])
        with patch('PiCoMonitor.pynvml', nv), p1, p2, p3:
            gpus = GpuMonitor().read()
        assert [g["n"] for g in gpus] == ["Radeon Vega Series"]
