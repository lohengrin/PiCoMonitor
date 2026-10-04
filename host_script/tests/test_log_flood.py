#!/usr/bin/env python3
"""
Tests for keeping persistent conditions from flooding the log: RepeatFilter and the
reconnect-loop fixes
"""

import logging
from types import SimpleNamespace
from unittest.mock import MagicMock, patch

import pytest

import PiCoMonitor
from PiCoMonitor import DataCollector, RepeatFilter, SerialCommunicationError


class Clock:
    def __init__(self):
        self.t = 1000.0

    def __call__(self):
        return self.t


def make_logger(name, **kwargs):
    """A private logger with a RepeatFilter on a fake clock, collecting what gets through"""
    clock = Clock()
    flt = RepeatFilter(clock=clock, **kwargs)
    lg = logging.getLogger(name)
    lg.handlers.clear()
    lg.filters.clear()
    lg.setLevel(logging.DEBUG)
    lg.propagate = False
    out = []

    class H(logging.Handler):
        def emit(self, record):
            out.append(record.getMessage())

    lg.addHandler(H())
    lg.addFilter(flt)
    return lg, clock, out


class TestRepeatFilter:
    def test_first_logged_then_suppressed_then_summarised(self):
        lg, clock, out = make_logger("t1", first_s=60, max_s=3600, reset_s=600)
        lg.warning("sensor missing")
        for _ in range(100):
            clock.t += 0.5
            lg.warning("sensor missing")
        assert out == ["sensor missing"]                       # 50 s elapsed: still suppressed
        clock.t += 15
        lg.warning("sensor missing")                           # >= 60 s after the first
        assert len(out) == 2 and out[1] == "sensor missing [100 similar messages suppressed]"

    def test_interval_doubles_up_to_the_maximum(self):
        lg, clock, out = make_logger("t2", first_s=10, max_s=40, reset_s=10_000)
        emitted_at = []
        for i in range(2000):
            clock.t = 1000 + i
            before = len(out)
            lg.error("persistent failure")
            if len(out) > before:
                emitted_at.append(i)
        gaps = [b - a for a, b in zip(emitted_at, emitted_at[1:])]
        assert gaps[:4] == [10, 20, 40, 40]                    # 10, 20, 40, then capped at 40
        assert emitted_at[0] == 0

    def test_digits_are_normalised_but_text_is_not(self):
        lg, clock, out = make_logger("t3")
        lg.info("Attempting to reconnect in 2 seconds...")
        lg.info("Attempting to reconnect in 4 seconds...")
        lg.info("Attempting to reconnect in 30 seconds...")
        assert out == ["Attempting to reconnect in 2 seconds..."]
        lg.info("Failed to get disk usage for /mnt/a")
        lg.info("Failed to get disk usage for /mnt/b")        # a different path is a different problem
        assert len(out) == 3

    def test_level_and_logger_are_part_of_the_key(self):
        lg, clock, out = make_logger("t4")
        lg.info("same text")
        lg.warning("same text")
        lg.error("same text")
        assert len(out) == 3

    def test_comes_back_at_once_after_a_quiet_period(self):
        lg, clock, out = make_logger("t5", first_s=60, reset_s=600)
        for _ in range(10):
            lg.warning("flaky")
        clock.t += 700                                          # silent for longer than reset_s
        lg.warning("flaky")
        assert len(out) == 2 and "9 similar messages suppressed" in out[1]
        lg.warning("flaky")
        assert len(out) == 2                                    # and suppression starts over

    def test_singular_summary(self):
        lg, clock, out = make_logger("t6", first_s=60)
        lg.info("x"); lg.info("x")
        clock.t += 61
        lg.info("x")
        assert out[-1] == "x [1 similar message suppressed]"

    def test_debug_is_never_filtered(self):
        lg, clock, out = make_logger("t7")
        for _ in range(50):
            lg.debug("noisy debug")
        assert len(out) == 50

    def test_message_with_percent_and_args_survives_annotation(self):
        lg, clock, out = make_logger("t8", first_s=10)
        lg.warning("100%% done, sensors: %s", ["a", "b"])
        lg.warning("100%% done, sensors: %s", ["a", "b"])       # suppressed
        lg.warning("100%% done, sensors: %s", ["a", "b"])       # suppressed
        clock.t += 11
        lg.warning("100%% done, sensors: %s", ["a", "b"])
        assert out[0] == "100% done, sensors: ['a', 'b']"
        assert out[1] == "100% done, sensors: ['a', 'b'] [2 similar messages suppressed]"

    def test_exception_traceback_only_on_emitted_records(self):
        lg, clock, out = make_logger("t9", first_s=60)
        records = []
        lg.handlers[0].emit = lambda r: records.append(r)
        for _ in range(5):
            try:
                raise RuntimeError("boom")
            except RuntimeError:
                lg.exception("Unexpected error in data collection loop")
        assert len(records) == 1 and records[0].exc_info is not None

    def test_bounded_memory(self):
        lg, clock, out = make_logger("t10", max_keys=20)
        for i in range(500):
            lg.info(f"unique message {'x' * i}")              # digits-free so every key differs
        assert len(lg.filters[0]._states) <= 20

    def test_independent_messages_do_not_affect_each_other(self):
        lg, clock, out = make_logger("t11")
        for _ in range(20):
            lg.warning("a")
        lg.warning("b")
        assert out == ["a", "b"]

    def test_real_logger_has_the_filter_exactly_once(self):
        flt = [f for f in PiCoMonitor.logger.filters if isinstance(f, RepeatFilter)]
        assert len(flt) == 1
        PiCoMonitor.LogSetup(PiCoMonitor.logger.name)           # setting up again must not stack filters
        assert len([f for f in PiCoMonitor.logger.filters if isinstance(f, RepeatFilter)]) == 1


class TestReconnectLoopLogging:
    def _no_sleep(self, dc):
        dc._wake.wait = lambda timeout=None: True

    def test_waiting_for_device_is_logged_once(self, caplog):
        caplog.set_level(logging.INFO)
        dc = DataCollector(None, 0.5)
        self._no_sleep(dc)
        calls = []

        def find():
            calls.append(1)
            if len(calls) >= 40:
                dc.stop()
            return None

        with patch('PiCoMonitor.PortDetector.find', side_effect=find):
            dc.work_loop(SimpleNamespace(visible=False))
        assert len(calls) == 40
        waiting = [r for r in caplog.records if "waiting for the device" in r.getMessage()]
        assert len(waiting) == 1

    def test_found_is_logged_once_when_the_device_appears(self, caplog):
        caplog.set_level(logging.INFO)
        dc = DataCollector(None, 0.5)
        self._no_sleep(dc)
        seq = iter([None, None, None, "/dev/ttyACM0", "/dev/ttyACM0"])

        def find():
            try:
                return next(seq)
            except StopIteration:
                dc.stop()
                return None

        manager = MagicMock()
        manager.return_value.__enter__.side_effect = SerialCommunicationError("busy")
        with patch('PiCoMonitor.PortDetector.find', side_effect=find), patch('PiCoMonitor.SerialPortManager', manager):
            dc.work_loop(SimpleNamespace(visible=False))
        text = [r.getMessage() for r in caplog.records]
        assert sum("waiting for the device" in m for m in text) == 1
        assert sum("Pico found on /dev/ttyACM0" in m for m in text) == 1

    def test_connection_is_logged_once_not_twice(self, caplog, dummy_serial):
        caplog.set_level(logging.INFO)
        dc = DataCollector("/dev/ttyACM0", 0.5)
        self._no_sleep(dc)
        dummy_serial.name = "/dev/ttyACM0"

        manager = MagicMock()
        manager.return_value.__enter__.return_value = dummy_serial
        manager.return_value.__exit__.return_value = False

        def stop_after_one_send(data, conn):
            dc.stop()
            return True

        with patch('PiCoMonitor.sys.platform', 'linux'), patch('PiCoMonitor.os.path.exists', return_value=True), \
                patch('PiCoMonitor.SerialPortManager', manager), \
                patch.object(dc, 'collect_system_data', return_value={}), \
                patch.object(dc, 'send_data_via_serial', side_effect=stop_after_one_send):
            dc.work_loop(SimpleNamespace(visible=False))
        # SerialPortManager (mocked here) owns the "Connected" message; the loop must not repeat it
        assert not [r for r in caplog.records if "Connected to serial port" in r.getMessage()]

    def test_persistent_error_in_the_data_loop_cannot_spin(self, dummy_serial):
        dc = DataCollector("/dev/ttyACM0", 0.5)
        waits = []
        errors = []

        def wait(timeout=None):
            waits.append(timeout)
            return True
        dc._wake.wait = wait

        def failing_collect():
            errors.append(1)
            if len(errors) >= 5:
                dc.stop()
            raise RuntimeError("sensor exploded")

        manager = MagicMock()
        manager.return_value.__enter__.return_value = dummy_serial
        manager.return_value.__exit__.return_value = False
        with patch('PiCoMonitor.sys.platform', 'linux'), patch('PiCoMonitor.os.path.exists', return_value=True), \
                patch('PiCoMonitor.SerialPortManager', manager), patch.object(dc, 'collect_system_data', side_effect=failing_collect):
            dc.work_loop(SimpleNamespace(visible=False))
        assert len(errors) == 5
        assert waits.count(1.0) >= 4                            # a pause after every failed cycle
