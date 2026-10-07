#!/usr/bin/env python3
"""
Tests for the UI commands (page / cycling mode) sent to the device and for the host-load
reductions (per-metric cache / send periods, compact frames)
"""

import json
from types import SimpleNamespace
from unittest.mock import Mock

import pytest

import PiCoMonitor
from PiCoMonitor import ArgumentValidator, DataCollector, Metric, SystemTrayIcon, config


class FakeClock:
    def __init__(self):
        self.t = 1000.0

    def __call__(self):
        return self.t


def collector(metrics, clock=None):
    return DataCollector('COM5', 0.5, metrics=metrics, clock=clock or FakeClock())


class TestMetricPeriods:
    def test_every_s_metric_is_sent_only_when_due(self):
        clock = FakeClock()
        reads = []
        dc = collector([Metric('FAST', lambda d: 1),
                        Metric('SLOW', lambda d: reads.append(1) or 7, every_s=10.0)], clock)
        assert dc.collect_system_data() == {'FAST': 1, 'SLOW': 7}      # first frame: everything
        clock.t += 0.5
        assert dc.collect_system_data() == {'FAST': 1}                  # not due: left out, not even read
        clock.t += 9.4
        assert dc.collect_system_data() == {'FAST': 1}
        clock.t += 0.2
        assert dc.collect_system_data() == {'FAST': 1, 'SLOW': 7}
        assert len(reads) == 2

    def test_cache_s_metric_is_reused_and_still_sent_every_frame(self):
        clock = FakeClock()
        values = iter([10, 20, 30])
        dc = collector([Metric('TEMP', lambda d: next(values), cache_s=2.0)], clock)
        assert dc.collect_system_data() == {'TEMP': 10}
        clock.t += 0.5
        assert dc.collect_system_data() == {'TEMP': 10}                 # cached, still in the frame
        clock.t += 1.6
        assert dc.collect_system_data() == {'TEMP': 20}                 # expired: read again

    def test_failing_cached_metric_sends_fallback_and_is_not_retried_every_frame(self):
        clock = FakeClock()
        calls = []
        def boom(d):
            calls.append(1)
            raise RuntimeError("sensor")
        dc = collector([Metric('TEMP', boom, fallback=None, cache_s=2.0)], clock)
        assert dc.collect_system_data() == {'TEMP': None}
        clock.t += 0.5
        dc.collect_system_data()
        assert len(calls) == 1

    def test_reconnect_makes_every_slow_metric_due_again(self):
        clock = FakeClock()
        dc = collector([Metric('SLOW', lambda d: 1, every_s=60.0)], clock)
        dc.collect_system_data()
        clock.t += 1
        assert dc.collect_system_data() == {}
        dc.on_connected()                                               # the firmware may have restarted
        assert dc.collect_system_data() == {'SLOW': 1}

    def test_reconnect_with_cached_metrics(self):
        clock = FakeClock()
        dc = collector([Metric('TEMP', lambda d: 5, cache_s=2.0), Metric('SLOW', lambda d: 1, every_s=60.0)], clock)
        dc.collect_system_data()
        dc.on_connected()
        assert dc.collect_system_data() == {'TEMP': 5, 'SLOW': 1}

    def test_default_metrics_slow_keys(self):
        by_key = {m.key: m for m in PiCoMonitor.default_metrics()}
        for key in ('DISKS', 'FREQ', 'LOAD', 'SWAP', 'UP'):
            assert by_key[key].every_s > 0
        for key in ('CPU', 'RAM', 'NET', 'IO'):                         # graphed: one point per frame
            assert by_key[key].every_s == 0

    def test_frame_is_compact_json(self):
        dc = collector([])
        conn = Mock(is_open=True)
        assert dc.send_data_via_serial({'A': [1, 2], 'B': {'c': 1}}, conn)
        assert conn.write.call_args[0][0] == b'{"A":[1,2],"B":{"c":1}}'


class TestUiCommands:
    def test_nothing_is_sent_unless_the_user_chose(self):
        dc = collector([Metric('A', lambda d: 1)])
        assert dc.collect_system_data() == {'A': 1}
        dc.on_connected()
        assert dc.collect_system_data() == {'A': 1}

    def test_page_goes_out_once(self):
        dc = collector([Metric('A', lambda d: 1)])
        dc.set_page('GPU')
        frame = dc.collect_system_data()
        assert frame['PAGE'] == 3
        dc._ui_sent(frame)
        assert 'PAGE' not in dc.collect_system_data() and 'CYCLE' not in dc.collect_system_data()

    def test_not_acknowledged_until_written(self):
        dc = collector([])
        dc.set_cycle(30)
        assert dc.collect_system_data()['CYCLE'] == 30
        assert dc.collect_system_data()['CYCLE'] == 30                  # write failed: still pending
        conn = Mock(is_open=True)
        dc.send_data_via_serial(dc.collect_system_data(), conn)
        assert 'CYCLE' not in dc.collect_system_data()

    def test_choice_is_resent_after_reconnect(self):
        dc = collector([])
        dc.set_page('network')
        dc.set_cycle(20)
        dc._ui_sent(dc.collect_system_data())
        assert dc.collect_system_data() == {}
        dc.on_connected()
        assert dc.collect_system_data() == {'PAGE': 1, 'CYCLE': 20}

    def test_page_after_cycle_turns_the_cycle_off(self):
        dc = collector([])
        dc.set_cycle(30)
        dc._ui_sent(dc.collect_system_data())
        dc.set_page('system')
        assert dc.collect_system_data() == {'PAGE': 2, 'CYCLE': 0}
        dc._ui_sent(dc.collect_system_data())
        dc.on_connected()
        assert dc.collect_system_data() == {'PAGE': 2}                  # a page implies cycling off

    def test_validation(self):
        dc = collector([])
        with pytest.raises(ValueError):
            dc.set_cycle(1)
        with pytest.raises(ValueError):
            dc.set_cycle(256)
        dc.set_cycle(0)
        with pytest.raises(KeyError):
            dc.set_page('nope')

    def test_cli_arguments(self):
        assert ArgumentValidator.validate_cycle(None)
        assert ArgumentValidator.validate_cycle(0)
        assert ArgumentValidator.validate_cycle(30)
        for bad in (1, 256, -5):
            with pytest.raises(ValueError):
                ArgumentValidator.validate_cycle(bad)

    def test_argparse(self, monkeypatch):
        app = PiCoMonitor.Application()
        monkeypatch.setattr('sys.argv', ['PiCoMonitor.py', '--page', 'gpu', '--cycle'])
        app.parse_arguments()
        assert app.args.page == 'gpu' and app.args.cycle == config.DEFAULT_CYCLE_S
        monkeypatch.setattr('sys.argv', ['PiCoMonitor.py', '--cycle', '10'])
        app.parse_arguments()
        assert app.args.cycle == 10 and app.args.page is None
        monkeypatch.setattr('sys.argv', ['PiCoMonitor.py'])
        app.parse_arguments()
        assert app.args.cycle is None and app.args.page is None


class TestTray:
    def test_delay_choices_include_two_seconds(self):
        assert 2.0 in config.DELAY_CHOICES and config.DEFAULT_DELAY == 0.5

    def test_tray_changes_reach_the_collector(self):
        app = PiCoMonitor.Application()
        app.args = SimpleNamespace(port=None, delay=1.0, no_tray=True, page='system', cycle=None)
        app.initialize_components()
        assert app.data_collector.delay == 1.0
        assert app.data_collector.collect_system_data.__self__ is app.data_collector
        # the command line page is sent to the device
        assert app.data_collector._ui_pending.get('PAGE') == 2
        tray = SystemTrayIcon()
        tray.on_delay, tray.on_page, tray.on_cycle = (app.data_collector.set_delay, app.data_collector.set_page,
                                                       app.data_collector.set_cycle)
        tray.set_delay(2.0)
        tray.set_cycle(10)
        tray.set_page('gpu')
        assert app.data_collector.delay == 2.0
        assert app.data_collector._ui_wanted == {'PAGE': 3}             # choosing a page ends the cycling mode
        assert tray.page == 'gpu' and tray.cycle == 0
