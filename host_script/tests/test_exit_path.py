#!/usr/bin/env python3
"""
Tests for the exit path: tray Exit, Ctrl+C / SIGTERM, serial write timeout and the forced exit
"""

import signal
import sys
import threading
import types
from types import SimpleNamespace
from unittest.mock import MagicMock, Mock, patch

import pytest

import PiCoMonitor
from PiCoMonitor import Application, DataCollector, SerialPortManager, SystemTrayIcon, config


def make_tray(module="pystray._appindicator"):
    tray = SystemTrayIcon()
    icon = MagicMock()
    type(icon).__module__ = module
    tray.icon = icon
    return tray, icon


class TestTrayExit:
    def test_exit_action_stops_the_collector_through_on_exit(self):
        tray, icon = make_tray()
        called = []
        tray.on_exit = lambda: called.append(1)
        tray.exit_action()
        assert called == [1] and tray.contflag is False
        assert icon.visible is False
        icon.stop.assert_called()

    def test_icon_is_stopped_even_if_on_exit_fails(self, caplog):
        tray, icon = make_tray()
        tray.on_exit = Mock(side_effect=RuntimeError("boom"))
        tray.exit_action()
        icon.stop.assert_called()
        assert "Error while stopping the application" in caplog.text

    def test_exit_action_without_hook_still_works(self):
        tray, icon = make_tray()
        tray.exit_action()
        icon.stop.assert_called()

    def test_stop_icon_is_safe_to_repeat_and_to_fail(self):
        tray, icon = make_tray()
        icon.stop.side_effect = RuntimeError("already stopped")
        tray.stop_icon()
        tray.stop_icon()                                         # must not raise
        SystemTrayIcon().stop_icon()                            # no icon at all: no-op


class TestShutdown:
    def make_app(self):
        app = Application()
        app.data_collector = MagicMock()
        app.tray_icon = MagicMock()
        return app

    def test_stops_collector_and_tray_exactly_once(self):
        app = self.make_app()
        app.shutdown("a")
        app.shutdown("b")
        app.shutdown("c")
        app.data_collector.stop.assert_called_once()
        app.tray_icon.stop_icon.assert_called_once()

    def test_works_without_tray_or_collector(self):
        app = Application()
        app.shutdown("nothing initialised")                     # must not raise

    def test_from_several_threads_stops_once(self):
        app = self.make_app()
        threads = [threading.Thread(target=app.shutdown, args=("t",)) for _ in range(8)]
        [t.start() for t in threads]
        [t.join() for t in threads]
        app.data_collector.stop.assert_called_once()

    def test_tray_exit_is_wired_to_shutdown(self):
        app = Application()
        app.args = SimpleNamespace(delay=0.5, port=None, no_tray=False)
        fake = MagicMock()
        with patch('PiCoMonitor.SystemTrayIcon', return_value=fake):
            app.initialize_components()
        assert app.tray_icon is fake
        fake.on_exit()                                          # what exit_action() calls
        assert app.data_collector.contflag is False             # the collector's thread can end
        fake.on_signal(signal.SIGINT)
        fake.stop_icon.assert_called_once()

    def test_python_signal_handler_shuts_down(self):
        app = self.make_app()
        installed = {}
        with patch('PiCoMonitor.signal.signal', side_effect=lambda s, h: installed.__setitem__(s, h)):
            app.setup_signal_handlers()
        assert set(installed) == {signal.SIGINT, signal.SIGTERM}
        installed[signal.SIGINT](signal.SIGINT, None)
        app.data_collector.stop.assert_called_once()
        app.tray_icon.stop_icon.assert_called_once()

    def test_run_shuts_down_when_the_tray_loop_returns(self):
        app = self.make_app()
        app.args = SimpleNamespace(delay=0.5, port=None)
        app.run()
        app.tray_icon.run.assert_called_once()
        app.data_collector.stop.assert_called_once()            # the loop ended: nothing may keep running

    def test_stopped_collector_loop_returns_promptly(self):
        dc = DataCollector(None, 0.5)
        dc._wake.wait = lambda timeout=None: True
        with patch('PiCoMonitor.PortDetector.find', side_effect=lambda: (dc.stop(), None)[1]):
            dc.work_loop(SimpleNamespace(visible=False))        # returns instead of looping forever


def fake_glib():
    glib = types.SimpleNamespace(PRIORITY_HIGH=-100, SOURCE_REMOVE=False, unix_signal_add=MagicMock())
    return glib, types.SimpleNamespace(repository=types.SimpleNamespace(GLib=glib))


class TestGlibSignals:
    def run_with(self, tray, glib_modules):
        with patch.dict(sys.modules, {"gi": glib_modules, "gi.repository": glib_modules.repository,
                                      "gi.repository.GLib": glib_modules.repository.GLib}):
            return tray._install_glib_signal_handlers()

    def test_sigint_and_sigterm_are_dispatched_by_the_loop(self):
        tray, _ = make_tray()
        seen = []
        tray.on_signal = seen.append
        glib, mods = fake_glib()
        assert self.run_with(tray, mods) is True
        calls = glib.unix_signal_add.call_args_list
        assert [c.args[1] for c in calls] == [int(signal.SIGINT), int(signal.SIGTERM)]
        assert all(c.args[0] == glib.PRIORITY_HIGH for c in calls)
        assert calls[0].args[2]() is False                      # one shot: SOURCE_REMOVE
        assert seen == [signal.SIGINT]

    def test_not_installed_for_other_backends(self):
        tray, _ = make_tray(module="pystray._win32")
        tray.on_signal = lambda s: None
        glib, mods = fake_glib()
        assert self.run_with(tray, mods) is False
        glib.unix_signal_add.assert_not_called()

    def test_not_installed_without_a_callback_or_without_gi(self):
        tray, _ = make_tray()
        glib, mods = fake_glib()
        assert self.run_with(tray, mods) is False               # no on_signal
        tray.on_signal = lambda s: None
        with patch.dict(sys.modules, {"gi": None, "gi.repository": None}):
            assert tray._install_glib_signal_handlers() is False   # import fails: no crash

    def test_run_installs_the_handlers_before_the_work_function(self):
        tray, icon = make_tray()
        order = []
        tray._install_glib_signal_handlers = lambda: order.append("signals")
        work = lambda ic: order.append("work")
        icon.run.side_effect = lambda setup: setup(icon)        # what pystray does once its loop is up
        tray.run(work)
        assert order == ["signals", "work"]


class TestForcedExit:
    def test_stuck_thread_forces_the_exit(self):
        release = threading.Event()
        t = threading.Thread(target=release.wait, name="stuck-writer")
        t.start()
        try:
            with patch('PiCoMonitor.os._exit') as exit_, patch('PiCoMonitor.logging.shutdown') as shut:
                Application().ensure_exit(grace=0.05)
            exit_.assert_called_once_with(0)
            shut.assert_called_once()                           # log files flushed first
        finally:
            release.set()
            t.join()

    def test_clean_state_does_not_force_anything(self):
        with patch('PiCoMonitor.os._exit') as exit_:
            Application().ensure_exit(grace=0.05)
        exit_.assert_not_called()

    def test_daemon_threads_are_ignored(self):
        release = threading.Event()
        t = threading.Thread(target=release.wait, daemon=True)
        t.start()
        try:
            with patch('PiCoMonitor.os._exit') as exit_:
                Application().ensure_exit(grace=0.05)
            exit_.assert_not_called()
        finally:
            release.set()
            t.join()


class TestSerialCannotHang:
    def test_port_opened_with_a_write_timeout(self):
        assert config.SERIAL_WRITE_TIMEOUT > 0
        with patch('PiCoMonitor.serial.Serial') as ser:
            with SerialPortManager("/dev/ttyACM0"):
                pass
        assert ser.call_args.kwargs["write_timeout"] == config.SERIAL_WRITE_TIMEOUT

    def test_pending_output_is_dropped_before_closing(self):
        calls = []
        conn = MagicMock(is_open=True)
        conn.reset_output_buffer.side_effect = lambda: calls.append("reset")
        conn.close.side_effect = lambda: calls.append("close")
        with patch('PiCoMonitor.serial.Serial', return_value=conn):
            with SerialPortManager("/dev/ttyACM0"):
                pass
        assert calls == ["reset", "close"]

    def test_close_works_without_reset_support(self):
        conn = MagicMock(is_open=True)
        conn.reset_output_buffer.side_effect = AttributeError
        with patch('PiCoMonitor.serial.Serial', return_value=conn):
            with SerialPortManager("/dev/ttyACM0"):
                pass
        conn.close.assert_called_once()

    def test_sending_never_waits_for_the_device_to_drain(self, dummy_serial):
        dummy_serial.flush = Mock(side_effect=AssertionError("flush() blocks forever on a stalled device"))
        assert DataCollector("COM5", 0.5).send_data_via_serial({"a": 1}, dummy_serial) is True
