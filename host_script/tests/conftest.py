#!/usr/bin/env python3
"""
Pytest fixtures for PiCoMonitor testing
"""

import pytest
from types import SimpleNamespace
from unittest.mock import Mock
import sys

# Add project root to path for imports
sys.path.insert(0, 'E:\\users\\Apps\\PicoMonitor')

@pytest.fixture(autouse=True)
def reset_log_repeat_filter():
    """The module logger's RepeatFilter remembers messages across tests; start each test clean"""
    import PiCoMonitor
    for f in PiCoMonitor.logger.filters:
        if isinstance(f, PiCoMonitor.RepeatFilter):
            f._states.clear()
    yield


@pytest.fixture
def mock_logger(caplog):
    """Fixture to capture log output using caplog"""
    return caplog


@pytest.fixture
def dummy_serial():
    """Minimal serial port stub for testing"""
    class DummySerial:
        def __init__(self):
            self.is_open = True
            self.closed = False
            self.name = "COM5"
        
        def write(self, data):
            return len(data)
        
        def flush(self):
            pass
        
        def close(self):
            self.closed = True
            self.is_open = False
    
    return DummySerial()


@pytest.fixture
def valid_args():
    """Valid argument namespace"""
    port = 'COM5' if sys.platform == 'win32' else '/dev/ttyACM0'
    return SimpleNamespace(delay=0.5, port=port, debug=False)


@pytest.fixture
def invalid_port_args():
    """Invalid port argument"""
    return SimpleNamespace(delay=0.5, port='XYZ', debug=False)


@pytest.fixture
def invalid_delay_args():
    """Invalid delay argument"""
    return SimpleNamespace(delay=0.05, port='COM5', debug=False)


@pytest.fixture
def icon_stub():
    """Minimal icon stub for testing"""
    class IconStub:
        def __init__(self):
            self.visible = False
        
        def stop(self):
            self.visible = False
    
    return IconStub()