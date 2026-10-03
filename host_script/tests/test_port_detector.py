#!/usr/bin/env python3
"""
Test cases for serial port auto-detection (PortDetector) and its use by DataCollector
"""

from types import SimpleNamespace
from unittest.mock import patch

import pytest

from PiCoMonitor import DataCollector, PortDetector, ArgumentValidator


def port(device, vid=None, pid=None, product=None, description="n/a"):
    return SimpleNamespace(device=device, vid=vid, pid=pid, product=product, description=description)


def with_ports(ports):
    return patch('PiCoMonitor.serial.tools.list_ports.comports', return_value=ports)


class TestPortDetector:
    def test_finds_pico_cdc(self):
        with with_ports([port('/dev/ttyS0'), port('/dev/ttyACM0', 0x2E8A, 0x000A)]):
            assert PortDetector.find() == '/dev/ttyACM0'

    def test_windows_style_names(self):
        with with_ports([port('COM1'), port('COM7', 0x2E8A, 0x000A)]):
            assert PortDetector.find() == 'COM7'

    def test_none_when_no_pico(self):
        with with_ports([port('COM1'), port('COM3', 0x0403, 0x6001)]):   # FTDI is not a Pico
            assert PortDetector.find() is None

    def test_none_when_no_ports(self):
        with with_ports([]):
            assert PortDetector.find() is None

    def test_bootsel_is_not_a_candidate(self):
        with with_ports([port('/dev/ttyACM0', 0x2E8A, 0x0003)]):
            assert PortDetector.find() is None

    def test_product_name_preferred_over_generic_cdc(self):
        with with_ports([port('/dev/ttyACM0', 0x2E8A, 0x000A),
                         port('/dev/ttyACM1', 0x2E8A, 0x000A, product='PiCoMonitor')]):
            assert PortDetector.find() == '/dev/ttyACM1'

    def test_natural_ordering_among_equals(self, caplog):
        with with_ports([port('/dev/ttyACM10', 0x2E8A, 0x000A), port('/dev/ttyACM2', 0x2E8A, 0x000A)]):
            assert PortDetector.find() == '/dev/ttyACM2'
        assert "Several Pico serial ports" in caplog.text

    def test_enumeration_failure_is_not_fatal(self):
        with patch('PiCoMonitor.serial.tools.list_ports.comports', side_effect=OSError('boom')):
            assert PortDetector.find() is None

    def test_format_ports_marks_choice_and_hides_noise(self):
        with with_ports([port('/dev/ttyS0'), port('/dev/ttyACM0', 0x2E8A, 0x000A, description='Pico')]):
            text = PortDetector.format_ports()
        assert 'ttyS0' not in text
        assert '/dev/ttyACM0' in text and '2E8A:000A' in text and 'auto-detect' in text


class TestAutoDetectIntegration:
    def test_none_port_is_valid(self):
        assert ArgumentValidator.validate_serial_port(None)
        assert DataCollector(None, 0.5).validate_serial_port()

    def test_work_loop_waits_for_device_then_stops(self):
        dc = DataCollector(None, 0.5)
        calls = []

        def find():
            calls.append(1)
            if len(calls) >= 2:
                dc.stop()
            return None

        with patch('PiCoMonitor.PortDetector.find', side_effect=find):
            dc._wake.wait = lambda t=None: True     # do not really sleep
            dc.work_loop(SimpleNamespace(visible=False))
        assert len(calls) >= 2

    def test_stop_wakes_the_wait_immediately(self):
        dc = DataCollector(None, 0.5)
        dc.stop()
        assert dc._wake.wait(0)       # event is set: reconnect back-off returns at once
