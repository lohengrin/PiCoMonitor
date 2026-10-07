#!/usr/bin/env python3
# coding=utf-8

import psutil
import json
import time
import serial
import serial.tools.list_ports
import sys
import argparse
import re
import threading
import collections
import glob
import shutil
import subprocess
from types import SimpleNamespace
try:
    import pystray
    from pystray import Menu, MenuItem
except Exception:  # no GUI/tray backend (headless Linux, e.g. Raspberry Pi OS Lite)
    pystray = None
    Menu = MenuItem = None
import logging
import logging.handlers
import os
import signal
import atexit
from typing import Callable, Dict, List, Any, Optional, Tuple
from dataclasses import dataclass
from contextlib import contextmanager

from PIL import Image, ImageDraw
from icon_art import draw_icon


# Optional NVIDIA GPU support (pure-python wrapper; the NVIDIA driver library is
# only needed at runtime, so this import succeeds on any machine)
try:
    import pynvml
except Exception:
    pynvml = None


def find_lhm_dll() -> Optional[str]:
    """Locate LibreHardwareMonitorLib.dll (Windows only): $PICOMONITOR_LHM_DLL,
    then LibreHardwareMonitor/ next to the script (or inside the PyInstaller bundle)."""
    candidates = []
    env = os.environ.get("PICOMONITOR_LHM_DLL")
    if env:
        candidates.append(env)
    for base in (getattr(sys, "_MEIPASS", None), os.path.dirname(os.path.abspath(__file__))):
        if base:
            candidates.append(os.path.join(base, "LibreHardwareMonitor", "LibreHardwareMonitorLib.dll"))
    for path in candidates:
        if os.path.isfile(path):
            return path
    return None


# LibreHardwareMonitor is Windows-only (.NET Framework 4.7.2); optional: without it
# (or without pythonnet) the temperature is simply unavailable.
Computer = None
SensorType = None
if sys.platform == "win32":
    try:
        import clr  # the pythonnet module (Windows only)
        _lhm_dll = find_lhm_dll()
        if _lhm_dll is None:
            raise FileNotFoundError("LibreHardwareMonitorLib.dll not found "
                                    "(set PICOMONITOR_LHM_DLL or put it in host_script/LibreHardwareMonitor/)")
        # its dependencies (HidSharp, DiskInfoToolkit, System.*.dll...) sit next to it
        sys.path.append(os.path.dirname(_lhm_dll))
        clr.AddReference(_lhm_dll)
        from LibreHardwareMonitor.Hardware import Computer, SensorType
    except Exception as _e:
        Computer = None
        SensorType = None
        print(f"LibreHardwareMonitor unavailable, no temperature: {_e}", file=sys.stderr)

# Configuration constants
@dataclass
class Config:
    DEFAULT_DELAY: float = 0.5
    DELAY_CHOICES = (0.25, 0.5, 1.0, 2.0)       # the tray menu
    # Page cycling (firmware side): period used by --cycle / the tray when none is given, and its limits
    DEFAULT_CYCLE_S: int = 30
    MIN_CYCLE_S: int = 2
    MAX_CYCLE_S: int = 255
    MAX_RETRY_ATTEMPTS: int = 3
    SERIAL_TIMEOUT: int = 1
    # A device that stops reading (frozen firmware, stalled hub) must not block the host forever:
    # writes fail after this long and the loop reconnects (and shutdown stays possible)
    SERIAL_WRITE_TIMEOUT: float = 2.0
    # After the tray / signal shutdown, wait this long for threads before forcing the exit
    EXIT_GRACE_S: float = 3.0
    BAUD_RATE: int = 19200
    LOG_FILE: str = "pico_monitor.log"
    MAX_LOG_SIZE: int = 1048576  # 1MB
    LOG_BACKUP_COUNT: int = 3
    # Repeated log messages (see RepeatFilter): a message that keeps coming back is logged
    # at once, then again after LOG_REPEAT_FIRST_S, then at doubling intervals up to
    # LOG_REPEAT_MAX_S; each of those re-emissions says how many were suppressed.
    LOG_REPEAT_FIRST_S: float = 60.0
    LOG_REPEAT_MAX_S: float = 3600.0
    LOG_REPEAT_RESET_S: float = 600.0     # silent for this long -> the next one is logged at once
    LOG_REPEAT_MAX_KEYS: int = 500

config = Config()

# USB identification (Raspberry Pi VID; pico-sdk's stdio_usb CDC default PID)
PICO_VID = 0x2E8A
PICO_CDC_PIDS = (0x000A,)
PICO_BOOTSEL_PIDS = (0x0003, 0x000F)   # ROM bootloader: mass storage only, no serial
APP_PRODUCT = "PiCoMonitor"            # preferred if the firmware ever reports it

# Error classes
class HardwareMonitorError(Exception):
    """Custom exception for hardware monitoring failures"""
    pass

class SerialCommunicationError(Exception):
    """Custom exception for serial communication failures"""
    pass

class DataCollectionError(Exception):
    """Custom exception for data collection failures"""
    pass

class SerialPortManager:
    """Context manager for serial port handling"""
    
    def __init__(self, port: str, baudrate: int = config.BAUD_RATE, timeout: int = config.SERIAL_TIMEOUT):
        self.port = port
        self.baudrate = baudrate
        self.timeout = timeout
        self.serial_connection = None
    
    def __enter__(self):
        """Open serial connection"""
        try:
            self.serial_connection = serial.Serial(
                self.port,
                baudrate=self.baudrate,
                timeout=self.timeout,
                write_timeout=config.SERIAL_WRITE_TIMEOUT
            )
            logger.info(f'Connected to serial port: {self.serial_connection.name} at {self.baudrate} baud')
            return self.serial_connection
        except (serial.SerialException, OSError, PermissionError) as e:
            logger.error(f'Failed to open serial port {self.port}: {e}')
            if isinstance(e, PermissionError) or 'Permission denied' in str(e):
                if sys.platform.startswith("linux"):
                    logger.error("Permission denied: add your user to the 'dialout' group "
                                 "(sudo usermod -aG dialout $USER, then log in again) or use a udev rule")
            raise SerialCommunicationError(f'Serial port connection failed: {e}')
    
    def __exit__(self, exc_type, exc_val, exc_tb):
        """Close serial connection"""
        if self.serial_connection and self.serial_connection.is_open:
            try:
                # Drop unsent data first: closing a tty with pending output can wait for the
                # driver's closing delay (tens of seconds) when the device is not reading
                try:
                    self.serial_connection.reset_output_buffer()
                except Exception:
                    pass
                self.serial_connection.close()
                logger.info("Serial port closed successfully")
            except Exception as e:
                logger.error(f"Error closing serial port: {e}")
        self.serial_connection = None

def _natural_key(text: str):
    return [int(t) if t.isdigit() else t.lower() for t in re.split(r'(\d+)', text)]


class PortDetector:
    """Find the Pico's USB serial port (COMx on Windows, /dev/ttyACMx on Linux)"""

    @staticmethod
    def candidates() -> List[Tuple[int, str, str]]:
        """Candidate ports as (rank, device, description), best first.
        Rank 0: product name is PiCoMonitor; 1: Pico CDC id; 2: other Raspberry Pi
        serial device. ROM-bootloader ids (no serial) are excluded."""
        found = []
        for p in serial.tools.list_ports.comports():
            if p.vid != PICO_VID or p.pid in PICO_BOOTSEL_PIDS:
                continue
            if APP_PRODUCT.lower() in (p.product or "").lower():
                rank = 0
            elif p.pid in PICO_CDC_PIDS:
                rank = 1
            else:
                rank = 2
            found.append((rank, p.device, p.description or ""))
        found.sort(key=lambda c: (c[0], _natural_key(c[1])))
        return found

    @staticmethod
    def find() -> Optional[str]:
        """Best candidate port, or None"""
        try:
            cands = PortDetector.candidates()
        except Exception as e:
            logger.error(f"Serial port enumeration failed: {e}")
            return None
        if len(cands) > 1:
            logger.warning("Several Pico serial ports found %s, using %s (select one with --port)",
                           [c[1] for c in cands], cands[0][1])
        return cands[0][1] if cands else None

    @staticmethod
    def format_ports() -> str:
        """Human-readable list of all serial ports, marking the auto-detect choice"""
        chosen = PortDetector.find()
        lines = []
        for p in sorted(serial.tools.list_ports.comports(), key=lambda p: _natural_key(p.device)):
            if p.vid is None and (p.description or "n/a") == "n/a":
                continue   # legacy/unused UARTs (/dev/ttyS*) are just noise
            ids = f"{p.vid:04X}:{p.pid:04X}" if p.vid is not None and p.pid is not None else "----:----"
            mark = "  <- auto-detect" if p.device == chosen else ""
            lines.append(f"{p.device:<16} {ids}  {p.description}{mark}")
        return "\n".join(lines) if lines else "(no serial ports found)"


class RepeatFilter(logging.Filter):
    """Keeps a persistent condition from flooding the log. Messages at INFO and
    above that repeat (same level and logger, same text once digits are
    normalised, so "retry in 4 s" and "retry in 8 s" are one message) are
    logged the first time, then re-logged only after an interval that doubles
    each time (first_s, 2*first_s, ... up to max_s) and says how many were
    suppressed. A message that stays away for reset_s is logged at once again.
    DEBUG messages are never filtered (they are only on when asked for)."""

    _DIGITS = re.compile(r"\d+(?:\.\d+)?")

    class _State:
        __slots__ = ("last_seen", "last_emit", "interval", "suppressed")

        def __init__(self, now: float, interval: float):
            self.last_seen = self.last_emit = now
            self.interval = interval
            self.suppressed = 0

    def __init__(self, first_s: float = 60.0, max_s: float = 3600.0, reset_s: float = 600.0,
                 max_keys: int = 500, clock: Callable[[], float] = time.monotonic):
        super().__init__()
        self.first_s, self.max_s, self.reset_s, self.max_keys = first_s, max_s, reset_s, max_keys
        self._clock = clock
        self._lock = threading.Lock()
        self._states: "collections.OrderedDict[Tuple[int, str, str], RepeatFilter._State]" = collections.OrderedDict()

    @staticmethod
    def _annotate(record: logging.LogRecord, suppressed: int):
        if suppressed:
            record.msg = (f"{record.getMessage()} "
                          f"[{suppressed} similar message{'s' if suppressed > 1 else ''} suppressed]")
            record.args = ()

    def filter(self, record: logging.LogRecord) -> bool:
        if record.levelno < logging.INFO:
            return True
        key = (record.levelno, record.name, self._DIGITS.sub("#", record.getMessage()))
        now = self._clock()
        with self._lock:
            state = self._states.get(key)
            if state is None or now - state.last_seen >= self.reset_s:
                suppressed = state.suppressed if state else 0
                self._states[key] = self._State(now, self.first_s)
                self._states.move_to_end(key)
                if len(self._states) > self.max_keys:
                    self._states.popitem(last=False)
                self._annotate(record, suppressed)
                return True
            state.last_seen = now
            self._states.move_to_end(key)
            if now - state.last_emit >= state.interval:
                suppressed, state.suppressed = state.suppressed, 0
                state.last_emit = now
                state.interval = min(state.interval * 2, self.max_s)
                self._annotate(record, suppressed)
                return True
            state.suppressed += 1
            return False


# Configure logging with rotation
class LogSetup:
    def __init__(self, name: str = __name__):
        self.logger = logging.getLogger(name)
        self._setup_logging()
    
    def _setup_logging(self):
        """Setup logging with file rotation and console output"""
        try:
            # Create logs directory if it doesn't exist
            log_dir = "logs"
            os.makedirs(log_dir, exist_ok=True)
            
            # Configure file handler with rotation
            file_handler = logging.handlers.RotatingFileHandler(
                os.path.join(log_dir, config.LOG_FILE),
                maxBytes=config.MAX_LOG_SIZE,
                backupCount=config.LOG_BACKUP_COUNT
            )
            
            # Configure console handler
            console_handler = logging.StreamHandler()
            
            # Set up formatter
            formatter = logging.Formatter(
                '%(asctime)s - %(name)s - %(levelname)s - %(message)s'
            )
            
            # Apply formatter to handlers
            file_handler.setFormatter(formatter)
            console_handler.setFormatter(formatter)
            
            # Add handlers to logger
            self.logger.addHandler(file_handler)
            self.logger.addHandler(console_handler)
            
            # Persistent conditions must not flood the log (applied once per record, before the handlers)
            if not any(isinstance(f, RepeatFilter) for f in self.logger.filters):
                self.logger.addFilter(RepeatFilter(config.LOG_REPEAT_FIRST_S, config.LOG_REPEAT_MAX_S,
                                                   config.LOG_REPEAT_RESET_S, config.LOG_REPEAT_MAX_KEYS))

            # Set default level
            self.logger.setLevel(logging.INFO)
            
            self.logger.info("Logging system initialized")
            
        except Exception as e:
            # Fallback to basic logging if setup fails
            logging.basicConfig(
                level=logging.INFO,
                format='%(asctime)s - %(levelname)s - %(message)s',
                handlers=[logging.StreamHandler()]
            )
            self.logger = logging.getLogger(__name__)
            self.logger.error(f"Advanced logging setup failed, using basic logging: {e}")
    
    def set_debug_level(self):
        """Enable debug level logging"""
        self.logger.setLevel(logging.DEBUG)
        self.logger.debug("Debug logging enabled")

# Initialize logger
logger_setup = LogSetup()
logger = logger_setup.logger

class HardwareMonitor:
    """Temperature through LibreHardwareMonitor (Windows only)"""

    # CPU temperature sensor names, best first: Intel, AMD, then generic core values
    _CPU_SENSORS = ('cpu package', 'core (tctl/tdie)', 'core (tctl)', 'core (tdie)', 'core average', 'core max')
    _GPU_SENSORS = ('gpu core',)

    def __init__(self):
        self.computer = None
        if sys.platform == "win32":
            self._initialize()

    def _initialize(self):
        """Initialize hardware monitoring with error handling (Windows only)"""
        try:
            self.computer = Computer()
            self.computer.IsCpuEnabled = True
            self.computer.IsGpuEnabled = True
            self.computer.Open()
            logger.info("Hardware monitoring initialized successfully")
        except Exception as e:
            logger.error(f"Failed to initialize hardware monitoring: {e}")
            self.computer = None

    @staticmethod
    def _temperatures(hardware) -> List[Tuple[str, float]]:
        """(lower-case name, value) of the valid temperature sensors of a hardware
        and its sub-hardware. 0 is how LHM reports a CPU sensor it cannot read
        (no admin rights / driver), so it is treated as missing."""
        out = []
        sensors = list(hardware.Sensors)
        for sub in hardware.SubHardware:
            sub.Update()
            sensors.extend(sub.Sensors)
        for sensor in sensors:
            try:
                if SensorType is not None and sensor.SensorType != SensorType.Temperature:
                    continue
                if "/temperature" not in str(sensor.Identifier):
                    continue
                value = sensor.Value
                if value is not None and float(value) > 0:
                    out.append((str(sensor.Name).lower(), float(value)))
            except Exception as e:
                logger.debug(f"Unreadable sensor: {e}")
        return out

    @staticmethod
    def _pick(temps: List[Tuple[str, float]], preferred: Tuple[str, ...]) -> Optional[float]:
        """The first preferred sensor present, else the first reading"""
        for name in preferred:
            for label, value in temps:
                if label == name:
                    return value
        return temps[0][1] if temps else None

    def get_temperature(self) -> Optional[float]:
        """CPU temperature, or the GPU one when the CPU cannot be read
        (LibreHardwareMonitor needs admin rights for most CPU sensors)"""
        if sys.platform != "win32" or self.computer is None:
            logger.warning("Hardware monitoring not available on this platform")
            return None

        try:
            cpu, gpu = None, None
            for hardware in self.computer.Hardware:
                hardware.Update()
                kind = str(hardware.HardwareType)
                temps = self._temperatures(hardware)
                if kind == "Cpu":
                    cpu = cpu if cpu is not None else self._pick(temps, self._CPU_SENSORS)
                elif kind.startswith("Gpu"):
                    gpu = gpu if gpu is not None else self._pick(temps, self._GPU_SENSORS)
            if cpu is not None:
                return cpu
            if gpu is not None:
                return gpu
            logger.warning("No temperature sensor found in LibreHardwareMonitor")
            return None
        except Exception as e:
            logger.warning(f"Failed to get temperature: {e}")
            return None

    def close(self):
        """Release LibreHardwareMonitor's drivers"""
        if self.computer is not None:
            try:
                self.computer.Close()
            except Exception:
                pass
            self.computer = None

    def is_available(self) -> bool:
        """Check if hardware monitoring is available"""
        return sys.platform == "win32" and self.computer is not None

# Initialize hardware monitor
hardware_monitor = HardwareMonitor()
atexit.register(hardware_monitor.close)

class SystemMonitor:
    """Class to handle system monitoring functionality"""
    
    @staticmethod
    def _filter_disk_partition(partition) -> bool:
        """Filter disk partitions based on platform and filesystem type"""
        try:
            fstype = partition.fstype
            path = partition.mountpoint
            
            if sys.platform.startswith("linux"):
                # Linux: keep only ext4, filter out /var/* (snap)
                if fstype != "ext4":
                    return False
                if path.startswith("/var/"):
                    return False
            else:
                # Windows: keep only NTFS
                if fstype != "NTFS":
                    return False
            return True
        except Exception as e:
            logger.warning(f"Error filtering partition {partition.mountpoint}: {e}")
            return False
    
    @staticmethod
    def _get_disk_label(path: str) -> str:
        """Return a concise label for a disk partition.
        * Windows → drive letter and colon (e.g. "C:")
        * Linux   → strip leading "/mnt/" if present
        """
        if sys.platform.startswith("linux"):
            if path.startswith("/mnt/"):
                return path.replace("/mnt/", "")
            return path
        # Windows – typical mount point looks like "C:\\" or "D:\\"
        # Return the drive letter (upper-cased) followed by the colon
        if len(path) >= 2 and path[1] == ":":
            return path[0].upper() + ":"
        # Fallback – return the raw path if it does not match the expected pattern
        return path
    
    @staticmethod
    def get_disk_usage() -> List[Dict[str, float]]:
        """Get disk usage information"""
        try:
            partitions = psutil.disk_partitions()
            data = []
            
            for partition in partitions:
                try:
                    if not SystemMonitor._filter_disk_partition(partition):
                        continue
                    
                    path = partition.mountpoint
                    label = SystemMonitor._get_disk_label(path)
                    
                    # Get usage information
                    usage = psutil.disk_usage(path)
                    total_gb = usage.total / 1000000000
                    used_gb = usage.used / 1000000000
                    
                    disk_info = {
                        "path": label,
                        "total": float("{0:.2f}".format(total_gb)),
                        "used": float("{0:.2f}".format(used_gb))
                    }
                    data.append(disk_info)
                    
                except (psutil.Error, PermissionError, OSError) as e:
                    logger.warning(f"Failed to get disk usage for {path}: {e}")
                    continue
            
            return data
            
        except Exception as e:
            logger.error(f"Failed to get disk usage information: {e}")
            return []
    
    @staticmethod
    def get_cpu_usage(delay: float) -> List[float]:
        """Get CPU usage information"""
        try:
            cpu_usage = psutil.cpu_percent(interval=delay, percpu=True)
            return cpu_usage
        except (psutil.Error, ValueError) as e:
            logger.error(f"Failed to get CPU usage: {e}")
            return []
    
    @staticmethod
    def get_cpu_temperature() -> Optional[float]:
        """Get CPU/GPU temperature based on platform"""
        try:
            if sys.platform.startswith("linux"):
                return SystemMonitor._get_linux_cpu_temp()
            elif sys.platform == "win32":
                return hardware_monitor.get_temperature()
            else:
                logger.warning("Temperature monitoring not supported on this platform")
                return None
        except Exception as e:
            logger.error(f"Failed to get CPU/GPU temperature: {e}")
            return None
    
    # psutil sensor names that report the CPU package temperature, best first:
    # Intel, AMD (k10temp/zenpower), Raspberry Pi (cpu_thermal), generic ACPI
    _CPU_SENSORS = ('coretemp', 'k10temp', 'zenpower', 'cpu_thermal', 'cpu-thermal', 'soc_thermal', 'acpitz')
    _PREFERRED_LABELS = ('package id 0', 'tctl', 'tdie', 'cpu')
    # Sensors that are certainly not the CPU/GPU (only skipped in the last-resort fallback)
    _NON_CPU_SENSORS = ('nvme', 'drivetemp', 'iwlwifi', 'ath', 'mt79', 'ddr', 'spd')

    @staticmethod
    def _pick_temperature(entries) -> Optional[float]:
        """Pick one reading from a sensor's entries: a package-level label if
        there is one, else the first valid reading (e.g. amdgpu's "edge").
        Entries are (label, current, ...)."""
        readings = [(str(e[0] or '').lower(), e[1]) for e in entries if e[1] is not None]
        if not readings:
            return None
        for label, value in readings:
            if label in SystemMonitor._PREFERRED_LABELS:
                return value
        return readings[0][1]

    @staticmethod
    def _get_linux_cpu_temp() -> Optional[float]:
        """CPU temperature on Linux (Ubuntu amd64 Intel/AMD, Raspberry Pi OS):
        known CPU sensors first, then a GPU sensor (amdgpu), then any other
        sensor that is not obviously storage/wifi/memory."""
        try:
            temps = psutil.sensors_temperatures()
            for name in SystemMonitor._CPU_SENSORS + ('amdgpu',):
                value = SystemMonitor._pick_temperature(temps.get(name) or [])
                if value is not None:
                    return value
            for name, entries in temps.items():
                if name.lower().startswith(SystemMonitor._NON_CPU_SENSORS):
                    continue
                value = SystemMonitor._pick_temperature(entries)
                if value is not None:
                    return value
            logger.warning("No usable temperature sensor found (sensors: %s)", sorted(temps) or "none")
            return None
        except (psutil.Error, AttributeError, KeyError, IndexError) as e:
            logger.warning(f"Failed to get Linux CPU temperature: {e}")
            return None
    
    @staticmethod
    def get_cpu_frequency() -> Optional[float]:
        """Current CPU frequency in MHz (None where psutil cannot tell, e.g. some VMs)"""
        try:
            f = psutil.cpu_freq()
            return _round(f.current, 0) if f and f.current else None
        except Exception as e:
            logger.debug(f"CPU frequency unavailable: {e}")
            return None

    @staticmethod
    def get_load_average() -> Optional[List[float]]:
        """1/5/15 minute load average (psutil emulates it on Windows)"""
        try:
            return [round(x, 2) for x in psutil.getloadavg()]
        except Exception as e:
            logger.debug(f"Load average unavailable: {e}")
            return None

    @staticmethod
    def get_swap_usage() -> Optional[float]:
        """Swap / page file usage in percent"""
        try:
            return _round(psutil.swap_memory().percent)
        except Exception as e:
            logger.debug(f"Swap usage unavailable: {e}")
            return None

    @staticmethod
    def get_uptime() -> Optional[int]:
        """Seconds since boot"""
        try:
            return max(0, int(time.time() - psutil.boot_time()))
        except Exception as e:
            logger.debug(f"Uptime unavailable: {e}")
            return None

    @staticmethod
    def get_memory_usage() -> Optional[float]:
        """Get memory usage percentage"""
        try:
            mem_percent = psutil.virtual_memory().percent
            return mem_percent
        except (psutil.Error, MemoryError) as e:
            logger.error(f"Failed to get memory usage: {e}")
            return None

# Remove old functions as they're now in SystemMonitor class

def _round(value: Optional[float], digits: int = 1) -> Optional[float]:
    return None if value is None else round(float(value), digits)


class RateTracker:
    """Per-second rates from psutil's cumulative network / disk I/O counters.
    The first call has no previous sample and returns None."""

    def __init__(self, clock: Callable[[], float] = time.monotonic):
        self._clock = clock
        self._prev: Dict[str, Tuple[float, Tuple[int, int]]] = {}

    def _rate(self, key: str, counters: Optional[Tuple[int, int]]) -> Optional[List[float]]:
        if counters is None:
            return None
        now = self._clock()
        prev = self._prev.get(key)
        self._prev[key] = (now, counters)
        if prev is None or now <= prev[0]:
            return None
        dt = now - prev[0]
        # KB/s; a counter reset / wrap (negative delta) counts as 0
        return [_round(max(0, c - p) / dt / 1000.0) for c, p in zip(counters, prev[1])]

    def network(self) -> Optional[List[float]]:
        """[download, upload] in KB/s over all interfaces"""
        try:
            n = psutil.net_io_counters()
            return self._rate('net', (n.bytes_recv, n.bytes_sent)) if n else None
        except Exception as e:
            logger.debug(f"Network counters unavailable: {e}")
            return None

    def disk_io(self) -> Optional[List[float]]:
        """[read, write] in KB/s over all disks"""
        try:
            d = psutil.disk_io_counters()
            return self._rate('io', (d.read_bytes, d.write_bytes)) if d else None
        except Exception as e:
            logger.debug(f"Disk I/O counters unavailable: {e}")
            return None


_PCI_IDS_PATHS = ("/usr/share/hwdata/pci.ids", "/usr/share/misc/pci.ids", "/usr/share/pci.ids")


def _short_gpu_name(raw: str, limit: int = 24) -> str:
    """"Cezanne [Radeon Vega Series / Radeon Vega Mobile Series]" -> "Radeon Vega Series" """
    if "[" in raw and "]" in raw:
        inner = raw[raw.index("[") + 1:raw.rindex("]")]
        raw = inner.split(" / ")[0].strip() or raw
    return raw.strip()[:limit]


def pci_device_name(vendor: int, device: int, paths: Optional[Tuple[str, ...]] = None) -> Optional[str]:
    """Look a PCI vendor/device id up in the system's pci.ids (Linux); None if unknown"""
    for path in (paths if paths is not None else _PCI_IDS_PATHS):
        try:
            in_vendor = False
            with open(path, encoding="utf-8", errors="replace") as f:
                for line in f:
                    if line.startswith("#") or not line.strip():
                        continue
                    if not line.startswith("\t"):                 # vendor line
                        if in_vendor:
                            return None                           # left the vendor without finding it
                        in_vendor = line.split()[0].lower() == "%04x" % vendor
                    elif in_vendor and not line.startswith("\t\t"):   # device line
                        parts = line.strip().split(None, 1)
                        if len(parts) == 2 and parts[0].lower() == "%04x" % device:
                            return parts[1]
        except OSError:
            continue
    return None


def find_nvidia_smi() -> Optional[str]:
    """nvidia-smi ships with the NVIDIA driver (Windows and Linux)"""
    path = shutil.which("nvidia-smi")
    if path:
        return path
    for p in (r"C:\Program Files\NVIDIA Corporation\NVSMI\nvidia-smi.exe", r"C:\Windows\System32\nvidia-smi.exe"):
        if os.path.isfile(p):
            return p
    return None


def parse_nvidia_smi(text: str) -> List[Dict[str, Any]]:
    """Parse `nvidia-smi --query-gpu=name,utilization.gpu,temperature.gpu,memory.used,memory.total
    --format=csv,noheader,nounits` (one line per GPU; fields may be "[N/A]")"""
    def num(field: str) -> Optional[float]:
        try:
            return float(field.strip())
        except ValueError:
            return None

    gpus = []
    for line in text.splitlines():
        fields = [f.strip() for f in line.split(",")]
        if len(fields) < 5 or not fields[0]:
            continue
        gpu: Dict[str, Any] = {"n": fields[0][:24]}
        load, temp, used, total = num(fields[1]), num(fields[2]), num(fields[3]), num(fields[4])
        if load is not None: gpu["l"] = load
        if temp is not None: gpu["t"] = temp
        if used is not None and total:
            gpu["mu"] = int(used)
            gpu["mt"] = int(total)
        gpus.append(gpu)
    return gpus


class GpuMonitor:
    """Optional GPU metrics for every GPU found. Sources, in this order:
      * NVIDIA via NVML (`pip install nvidia-ml-py`; Windows/Linux, needs the NVIDIA driver),
        else via `nvidia-smi` (ships with the driver, no Python package needed; polled at most
        every SMI_CACHE_S seconds because it starts a process);
      * AMD via sysfs (Linux amdgpu, discrete or integrated).
    read() returns a list with one dict per GPU (NVIDIA first), or None where there is no
    supported GPU (e.g. Raspberry Pi)."""

    MAX_GPUS = 4
    SMI_CACHE_S = 2.0

    def __init__(self, clock: Callable[[], float] = time.monotonic):
        self._clock = clock
        self._sources: Optional[List[Callable[[], List[Dict[str, Any]]]]] = None
        self._smi_path: Optional[str] = None
        self._smi_cache: List[Dict[str, Any]] = []
        self._smi_time = float("-inf")

    def read(self) -> Optional[List[Dict[str, Any]]]:
        if self._sources is None:
            self._sources = self._probe()
        gpus: List[Dict[str, Any]] = []
        for source in self._sources:
            try:
                gpus.extend(source())
            except Exception as e:
                logger.debug(f"GPU source failed: {e}")
        return gpus[:self.MAX_GPUS] or None

    # ---- probing -------------------------------------------------------------------------------
    def _probe(self) -> List[Callable[[], List[Dict[str, Any]]]]:
        sources: List[Callable[[], List[Dict[str, Any]]]] = []

        nvidia = self._probe_nvml() or self._probe_smi()
        if nvidia:
            sources.append(nvidia)

        if sys.platform.startswith("linux"):
            for card in sorted(glob.glob("/sys/class/drm/card[0-9]*/device")):
                if not re.fullmatch(r"card\d+", os.path.basename(os.path.dirname(card))):
                    continue                                     # connectors (card1-DP-1...)
                if os.path.exists(os.path.join(card, "gpu_busy_percent")):
                    name = self._amd_name(card)
                    sources.append(lambda d=card, n=name: [self._read_amd(d, n)])
                    logger.info(f"GPU monitoring: AMD {name} (sysfs {card})")
        if not sources:
            logger.info("GPU monitoring: no supported GPU found")
        return sources

    def _probe_nvml(self):
        if pynvml is None:
            return None
        try:
            pynvml.nvmlInit()
            handles = [pynvml.nvmlDeviceGetHandleByIndex(i) for i in range(pynvml.nvmlDeviceGetCount())]
        except Exception as e:
            logger.debug(f"NVML unavailable: {e}")
            return None
        if not handles:
            return None
        logger.info(f"GPU monitoring: {len(handles)} NVIDIA GPU(s) (NVML)")

        def read_all() -> List[Dict[str, Any]]:
            out = []
            for h in handles:
                try:                      # one GPU failing must not hide the others
                    out.append(self._read_nvidia(h))
                except Exception as e:
                    logger.debug(f"NVML read failed: {e}")
            return out
        return read_all

    def _probe_smi(self):
        path = find_nvidia_smi()
        if path is None:
            return None
        self._smi_path = path
        gpus = self._run_smi()
        if not gpus:
            return None
        self._smi_cache, self._smi_time = gpus, self._clock()      # the probe call is the first sample
        logger.info(f"GPU monitoring: {len(gpus)} NVIDIA GPU(s) via nvidia-smi"
                    + ("" if pynvml else " (install nvidia-ml-py for lower overhead: pip install nvidia-ml-py)"))
        return self._read_smi

    def _run_smi(self) -> List[Dict[str, Any]]:
        kwargs: Dict[str, Any] = {}
        if sys.platform == "win32":
            kwargs["creationflags"] = getattr(subprocess, "CREATE_NO_WINDOW", 0)   # no console flash
        try:
            res = subprocess.run(
                [self._smi_path, "--query-gpu=name,utilization.gpu,temperature.gpu,memory.used,memory.total",
                 "--format=csv,noheader,nounits"],
                capture_output=True, text=True, timeout=3, **kwargs)
            return parse_nvidia_smi(res.stdout) if res.returncode == 0 else []
        except (OSError, subprocess.SubprocessError) as e:
            logger.debug(f"nvidia-smi failed: {e}")
            return []

    def _read_smi(self) -> List[Dict[str, Any]]:
        now = self._clock()
        if now - self._smi_time >= self.SMI_CACHE_S:
            self._smi_time = now
            self._smi_cache = self._run_smi() or self._smi_cache   # keep the last values on a failed call
        return self._smi_cache

    # ---- readers -------------------------------------------------------------------------------
    @staticmethod
    def _read_nvidia(handle) -> Dict[str, Any]:
        name = pynvml.nvmlDeviceGetName(handle)
        if isinstance(name, bytes):
            name = name.decode(errors="replace")
        util = pynvml.nvmlDeviceGetUtilizationRates(handle)
        mem = pynvml.nvmlDeviceGetMemoryInfo(handle)
        temp = pynvml.nvmlDeviceGetTemperature(handle, pynvml.NVML_TEMPERATURE_GPU)
        return {"n": name[:24], "l": float(util.gpu), "t": float(temp),
                "mu": int(mem.used // (1024 * 1024)), "mt": int(mem.total // (1024 * 1024))}

    @staticmethod
    def _read_int(path: str) -> Optional[int]:
        try:
            with open(path) as f:
                return int(f.read().strip(), 0)
        except (OSError, ValueError):
            return None

    @staticmethod
    def _amd_name(device_dir: str) -> str:
        vendor = GpuMonitor._read_int(os.path.join(device_dir, "vendor"))
        device = GpuMonitor._read_int(os.path.join(device_dir, "device"))
        if vendor is not None and device is not None:
            raw = pci_device_name(vendor, device)
            if raw:
                return _short_gpu_name(raw)
        return "AMD GPU"

    @staticmethod
    def _read_amd(d: str, name: str) -> Dict[str, Any]:
        load = GpuMonitor._read_int(os.path.join(d, "gpu_busy_percent"))
        used = GpuMonitor._read_int(os.path.join(d, "mem_info_vram_used"))
        total = GpuMonitor._read_int(os.path.join(d, "mem_info_vram_total"))
        temp = None
        for hw in sorted(glob.glob(os.path.join(d, "hwmon", "hwmon*", "temp1_input"))):
            t = GpuMonitor._read_int(hw)
            if t is not None:
                temp = t / 1000.0
                break
        out: Dict[str, Any] = {"n": name}
        if load is not None: out["l"] = float(load)
        if temp is not None: out["t"] = float(temp)
        if used is not None and total:
            out["mu"] = used // (1024 * 1024)
            out["mt"] = total // (1024 * 1024)
        return out


class SystemTrayIcon:
    """Class to manage system tray icon and menu"""
    
    def __init__(self, title: str = 'PiCoMonitor - System Monitoring Tool'):
        self.title = title
        self.icon = None
        self.contflag = True
        self.delay = config.DEFAULT_DELAY
        self.current_delay = self.delay
        # What the user last picked in the menu (None: nothing chosen, the device keeps its own state)
        self.page: Optional[str] = None
        self.cycle: Optional[int] = None
        # Tell the data collector (set by Application)
        self.on_delay: Optional[Callable[[float], None]] = None
        self.on_page: Optional[Callable[[str], None]] = None
        self.on_cycle: Optional[Callable[[int], None]] = None
        # Called when the user picks Exit (set by Application: stops the data collector too)
        self.on_exit: Optional[Callable[[], None]] = None
        # Called from the GUI main loop on SIGINT/SIGTERM (GTK backends only, see run())
        self.on_signal: Optional[Callable[[int], None]] = None
    
    @staticmethod
    def create_image(width: int, height: int, color1: str, color2: str) -> Image.Image:
        """Generate an image and draw a pattern"""
        image = Image.new('RGB', (width, height), color1)
        dc = ImageDraw.Draw(image)
        dc.rectangle((width // 2, 0, width, height // 2), fill=color2)
        dc.rectangle((0, height // 2, width // 2, height), fill=color2)
        return image
    
    def create_menu(self) -> Menu:
        """Create system tray menu"""
        delays = [MenuItem(f'{d:.2f} s', (lambda d: lambda i: self.set_delay(d))(d),
                           checked=(lambda d: lambda i: self.get_delay(d))(d), radio=True)
                  for d in config.DELAY_CHOICES]
        pages = [MenuItem(name.capitalize() if name != 'gpu' else 'GPU', (lambda n: lambda i: self.set_page(n))(name),
                          checked=(lambda n: lambda i: self.page == n)(name), radio=True)
                 for name in DataCollector.PAGES]
        cycles = [MenuItem('Off' if c == 0 else f'{c} s', (lambda c: lambda i: self.set_cycle(c))(c),
                           checked=(lambda c: lambda i: self.cycle == c)(c), radio=True)
                  for c in (0, 10, config.DEFAULT_CYCLE_S, 60)]
        return Menu(
            *delays,
            Menu.SEPARATOR,
            MenuItem('Page', Menu(*pages)),
            MenuItem('Cycle pages', Menu(*cycles)),
            Menu.SEPARATOR,
            MenuItem('Exit', lambda: self.exit_action())
        )

    def set_delay(self, delay_value: float):
        """Set the data collection delay"""
        self.delay = delay_value
        if self.on_delay:
            self.on_delay(delay_value)
        logger.info(f"Delay set to {delay_value}s")

    def get_delay(self, delay_value: float) -> bool:
        """Check if current delay matches the given value"""
        return self.delay == delay_value

    def set_page(self, page: str):
        """Show a page on the device (this ends its cycling mode)"""
        self.page = page
        self.cycle = 0
        if self.on_page:
            self.on_page(page)

    def set_cycle(self, seconds: int):
        """Cycle through the pages every `seconds` (0 = off)"""
        self.cycle = seconds
        if self.on_cycle:
            self.on_cycle(seconds)

    def exit_action(self):
        """Handle application exit"""
        try:
            logger.info("User requested exit")
            self.contflag = False
            if self.on_exit is not None:
                try:
                    self.on_exit()          # stop the data collector: its thread keeps the process alive otherwise
                except Exception:
                    logger.exception("Error while stopping the application")
            if self.icon:
                self.icon.visible = False
                self.icon.stop()
            logger.info("Application shutdown initiated")
        except Exception as e:
            logger.error(f"Error during exit: {e}")
            sys.exit(1)
    
    def initialize(self):
        """Initialize system tray icon"""
        if pystray is None:
            raise RuntimeError("pystray/tray backend unavailable")
        self.icon = pystray.Icon('PiCoMonitor', icon=draw_icon(64))
        self.icon.menu = self.create_menu()
        self.icon.title = self.title
    
    def stop_icon(self):
        """Hide the icon and end its event loop (safe to call from any thread, more than once)"""
        if self.icon:
            try:
                self.icon.visible = False
                self.icon.stop()
            except Exception as e:
                logger.debug(f"Stopping the tray icon failed: {e}")

    def _uses_glib_loop(self) -> bool:
        """The AppIndicator / GTK pystray backends run a GLib main loop in the main thread"""
        module = type(self.icon).__module__ if self.icon else ""
        return module.startswith(("pystray._appindicator", "pystray._gtk", "pystray._util.gtk"))

    def _install_glib_signal_handlers(self) -> bool:
        """Route SIGINT/SIGTERM through the GLib main loop. The GTK backend resets SIGINT to the OS default
        when its loop starts (so Python handlers never run: Ctrl+C just kills the process, skipping the
        shutdown), and the main thread sits inside the loop where Python-level handlers cannot run. A GLib
        signal source is dispatched by the loop itself, immediately. Must be called after the loop started
        (see run()). Returns False where not applicable."""
        if self.on_signal is None or not self._uses_glib_loop():
            return False
        try:
            from gi.repository import GLib
        except Exception as e:
            logger.debug(f"GLib unavailable, no loop-level signal handling: {e}")
            return False

        def make(sig):
            def handler():
                self.on_signal(sig)
                return GLib.SOURCE_REMOVE       # one shot: shutting down
            return handler

        for sig in (signal.SIGINT, signal.SIGTERM):
            GLib.unix_signal_add(GLib.PRIORITY_HIGH, int(sig), make(sig))
        return True

    def run(self, work_function):
        """Run the system tray icon with the given work function (in pystray's setup thread)"""
        if not self.icon:
            return

        def setup(icon):
            # pystray runs this once its loop is up, i.e. after it reset SIGINT: install ours now
            self._install_glib_signal_handlers()
            work_function(icon)

        self.icon.run(setup)

@dataclass
class Metric:
    """One entry of the frame sent to the Pico. `collect(collector)` returns the
    value; if it raises, `fallback` is sent instead so one broken sensor never
    affects the others. Keys the firmware does not know are ignored by it."""
    key: str
    collect: Callable[['DataCollector'], Any]
    fallback: Any = None
    optional: bool = False   # omitted from the frame when unavailable (None) instead of sent as null
    # Host load: reading a sensor costs CPU/IO, most values change slowly. Two knobs (seconds, 0 = off):
    cache_s: float = 0.0     # reuse the last value this long; it is still sent with every frame
    every_s: float = 0.0     # read AND send only this often; the firmware keeps the last value in between


def default_metrics() -> List[Metric]:
    # Looked up through SystemMonitor at call time (patchable in tests)
    return [
        # Graphed values are sent with every frame (the graphs advance one point per frame)
        Metric('CPU', lambda dc: SystemMonitor.get_cpu_usage(dc.delay), []),   # blocks `delay` s: paces the loop
        Metric('TEMP', lambda dc: SystemMonitor.get_cpu_temperature(), None, cache_s=2.0),
        Metric('RAM', lambda dc: SystemMonitor.get_memory_usage(), None),
        # Slow values (info lists, disk bars) are sent when refreshed; the firmware remembers them
        Metric('DISKS', lambda dc: SystemMonitor.get_disk_usage(), [], every_s=30.0),
        # Optional extras, shown on the firmware's extra pages. Anything the
        # platform cannot provide is simply left out of the frame.
        Metric('NET', lambda dc: dc.rates.network(), optional=True),     # [down, up] KB/s
        Metric('IO', lambda dc: dc.rates.disk_io(), optional=True),      # [read, write] KB/s
        Metric('FREQ', lambda dc: SystemMonitor.get_cpu_frequency(), optional=True, every_s=2.0),   # MHz
        Metric('LOAD', lambda dc: SystemMonitor.get_load_average(), optional=True, every_s=5.0),    # [1, 5, 15 min]
        Metric('SWAP', lambda dc: SystemMonitor.get_swap_usage(), optional=True, every_s=5.0),      # %
        Metric('UP', lambda dc: SystemMonitor.get_uptime(), optional=True, every_s=10.0),           # seconds
        Metric('GPU', lambda dc: dc.gpu.read(), optional=True, cache_s=1.0),
    ]


class DataCollector:
    """Class to handle data collection and serial communication"""
    
    # Page names as the firmware numbers them (Pages::Id)
    PAGES = {'overview': 0, 'network': 1, 'system': 2, 'gpu': 3}

    def __init__(self, port: Optional[str], initial_delay: float, metrics: Optional[List[Metric]] = None,
                 clock: Callable[[], float] = time.monotonic):
        # port None = auto-detect (re-evaluated at every (re)connection attempt)
        self.port = port
        self.delay = initial_delay
        self.contflag = True
        self.serial_manager = None
        self.metrics = metrics if metrics is not None else default_metrics()
        self.rates = RateTracker()
        self.gpu = GpuMonitor()
        self._wake = threading.Event()
        self._clock = clock
        self._read_at: Dict[str, float] = {}      # metric key -> when it was last read
        self._cached: Dict[str, Any] = {}         # metric key -> last value (for cache_s metrics)
        # UI commands: what the user chose (re-sent after every (re)connection, the firmware may
        # have restarted) and what still has to go out. Nothing is sent unless the user chose.
        self._ui_lock = threading.Lock()
        self._ui_wanted: Dict[str, int] = {}
        self._ui_pending: Dict[str, int] = {}

    def set_delay(self, delay: float):
        """Period of the data frames (takes effect at the next frame)"""
        self.delay = delay

    def set_page(self, page: str):
        """Show a page ('overview', 'network', 'system', 'gpu'): ends the cycling mode"""
        index = self.PAGES[page.lower()]
        with self._ui_lock:
            self._ui_wanted.pop('CYCLE', None)
            self._ui_wanted['PAGE'] = index
            self._ui_pending.update(PAGE=index, CYCLE=0)
        logger.info(f"Page set to {page}")

    def set_cycle(self, seconds: int):
        """Page cycling mode: a new page every `seconds` (0 = off), starting from the current page"""
        seconds = int(seconds)
        if seconds and not (config.MIN_CYCLE_S <= seconds <= config.MAX_CYCLE_S):
            raise ValueError(f"Cycle period must be {config.MIN_CYCLE_S}-{config.MAX_CYCLE_S} s, got {seconds}")
        with self._ui_lock:
            self._ui_wanted['CYCLE'] = seconds
            self._ui_pending['CYCLE'] = seconds
        logger.info(f"Page cycling {'every %d s' % seconds if seconds else 'off'}")

    def on_connected(self):
        """A (new) connection: everything is due again, including the user's page / cycling choice"""
        self._read_at.clear()
        self._cached.clear()
        with self._ui_lock:
            self._ui_pending = dict(self._ui_wanted)

    def _read_metric(self, metric: Metric, now: float):
        """(value, fresh): fresh=False when a cached value is reused"""
        cached = self._cached.get(metric.key)
        if metric.cache_s and metric.key in self._cached and metric.key in self._read_at \
                and now - self._read_at[metric.key] < metric.cache_s:
            return cached, False
        try:
            value = metric.collect(self)
        except Exception as e:
            logger.error(f"Metric {metric.key} failed: {e}")
            value = metric.fallback
        self._read_at[metric.key] = now
        if metric.cache_s:
            self._cached[metric.key] = value
        return value, True

    def collect_system_data(self) -> Dict[str, Any]:
        """Collect system monitoring data: the keys of this frame (slow ones are left out until due)"""
        data = {}
        now = self._clock()
        for metric in self.metrics:
            if metric.every_s and metric.key in self._read_at and now - self._read_at[metric.key] < metric.every_s:
                continue
            value, _ = self._read_metric(metric, now)
            if value is None and metric.optional:
                continue
            data[metric.key] = value
        with self._ui_lock:
            data.update(self._ui_pending)
        return data

    def _ui_sent(self, data: Dict[str, Any]):
        """The frame went out: its UI commands are done (unless the user changed them meanwhile)"""
        with self._ui_lock:
            for key in ('PAGE', 'CYCLE'):
                if key in data and self._ui_pending.get(key) == data[key]:
                    del self._ui_pending[key]
    
    def send_data_via_serial(self, data: Dict[str, Any], serial_conn: serial.Serial):
        """Send data via serial connection"""
        try:
            serialized_data = json.dumps(data, separators=(',', ':'))
            if not serial_conn or not serial_conn.is_open:
                logger.warning("Serial port not open for writing")
                return False
            # Use UTF-8 to be safe with any characters
            serial_conn.write(serialized_data.encode('utf-8'))
            self._ui_sent(data)
            # No flush(): it waits for the device to drain the data with no timeout, which would
            # hang forever on a stalled device. write() is already bounded by write_timeout and
            # hands the data to the driver immediately.
            return True
        except (json.JSONDecodeError, UnicodeEncodeError, TypeError, ValueError) as e:
            logger.error(f"Failed to serialize or encode data: {e}")
            return False
        except serial.SerialTimeoutException:
            logger.warning("Serial write timeout occurred")
            return False
        except serial.SerialException as e:
            logger.error(f"Serial write failed (port lost?): {e}")
            # Propagate to outer loop for reconnection
            raise SerialCommunicationError(str(e))
        except Exception as e:
            logger.exception("Unexpected error while sending data via serial")
            raise
    
    def work_loop(self, icon):
        """Main work loop for data collection and transmission"""
        icon.visible = True
        
        # Validate serial port before starting (None = auto-detect)
        if not self.validate_serial_port():
            logger.error(f"Invalid serial port: {self.port}")
            return
        
        # Main loop (connect/reconnect) with exponential back‑off
        retry_delay = 2  # seconds, will double on each failure up to a max
        max_delay = 30
        waiting_for_device = False    # "waiting" is logged once, when the wait starts (and "found" when it ends)
        while self.contflag:
            # Auto-detect on every attempt: the device may be plugged in later or
            # change its port name
            port = self.port or PortDetector.find()
            if port is None:
                if not waiting_for_device:
                    logger.info("No Pico serial port found, waiting for the device...")
                    waiting_for_device = True
                self._wake.wait(min(retry_delay, 5))
                continue
            if waiting_for_device:
                logger.info(f"Pico found on {port}")
                waiting_for_device = False
            try:
                # Attempt to open serial port with context manager
                with SerialPortManager(port, config.BAUD_RATE, config.SERIAL_TIMEOUT) as serial_conn:
                    # (SerialPortManager already logged the connection)
                    # Reset back‑off after a successful connection
                    retry_delay = 2
                    self.on_connected()
                    # Data loop – collect and send until a serial error occurs or shutdown
                    while self.contflag:
                        try:
                            system_data = self.collect_system_data()
                            # send_data_via_serial may raise SerialCommunicationError on write failure
                            if not self.send_data_via_serial(system_data, serial_conn):
                                # Non‑fatal send failure (e.g., port closed) – break to reconnect
                                break
                            time.sleep(0.01)
                        except SerialCommunicationError as e:
                            logger.warning(f"Serial communication error during data send: {e}")
                            break  # exit inner loop to reconnect
                        except KeyboardInterrupt:
                            logger.info("User interrupted data collection")
                            self.contflag = False
                            break
                        except Exception as e:
                            logger.exception("Unexpected error in data collection loop")
                            # Continue collecting; do not break unless contflag cleared. Pause a
                            # moment so a persistent error cannot spin (and flood) at full speed
                            self._wake.wait(1.0)
                            continue
            except SerialCommunicationError as e:
                # SerialPortManager already logged the error; keep this one for diagnosis only
                logger.debug(f'Serial port connection failed: {e}')
            except Exception as e:
                logger.exception("Unexpected error in main loop")
            
            # Wait before reconnect unless shutting down
            if self.contflag:
                logger.info(f"Attempting to reconnect in {retry_delay} seconds...")
                self._wake.wait(retry_delay)
                retry_delay = min(retry_delay * 2, max_delay)
    
    def validate_serial_port(self) -> bool:
        """Validate that the serial port is accessible"""
        if self.port is None:      # auto-detect: nothing to check yet
            return True
        try:
            if sys.platform == "win32":
                # For Windows, basic validation
                if not self.port.startswith("COM"):
                    logger.error(f"Invalid COM port format: {self.port}")
                    return False
            else:
                # For Unix-like (Linux/macOS), check if the device file exists
                if not os.path.exists(self.port):
                    logger.error(f"Serial port {self.port} does not exist")
                    return False
            return True
        except Exception as e:
            logger.error(f"Error validating serial port: {e}")
            return False
    
    def stop(self):
        """Stop the data collection loop and wake any sleeping back‑off"""
        self.contflag = False
        self._wake.set()   # wake the reconnect/back-off wait immediately

# Remove old functions as they're now in DataCollector class

class ArgumentValidator:
    """Class to validate command line arguments"""
    
    @staticmethod
    def validate_delay(delay_value: float) -> bool:
        """Validate delay range"""
        if not (0.1 <= delay_value <= 10.0):
            raise ValueError(f"Delay must be between 0.1 and 10.0 seconds, got {delay_value}")
        return True
    
    @staticmethod
    def validate_cycle(seconds: Optional[int]) -> bool:
        """Page cycling period (None: not given, 0: off)"""
        if seconds is not None and seconds != 0 and not (config.MIN_CYCLE_S <= seconds <= config.MAX_CYCLE_S):
            raise ValueError(f"Cycle period must be 0 (off) or {config.MIN_CYCLE_S}-{config.MAX_CYCLE_S} seconds, got {seconds}")
        return True

    @staticmethod
    def validate_serial_port(port: Optional[str]) -> bool:
        """Validate serial port format (None = auto-detect)"""
        if port is None:
            return True
        if sys.platform == "win32":
            if not port.startswith("COM"):
                raise ValueError(f"Invalid serial port format for Windows: {port}")
        else:
            if not port.startswith("/dev/"):
                raise ValueError(f"Invalid serial port format: {port}")
        return True
    
    @staticmethod
    def validate_required_modules() -> bool:
        """Check if required modules are available"""
        try:
            import psutil
            import serial
            return True
        except ImportError as e:
            raise ImportError(f"Missing required module: {e}")
    
    @staticmethod
    def validate_all(args) -> bool:
        """Validate all arguments"""
        try:
            ArgumentValidator.validate_delay(args.delay)
            ArgumentValidator.validate_cycle(getattr(args, "cycle", None))
            ArgumentValidator.validate_serial_port(args.port)
            ArgumentValidator.validate_required_modules()
            
            logger.info("Arguments validation passed")
            return True
        except Exception as e:
            logger.error(f"Argument validation failed: {e}")
            return False

# Remove old functions as they're now in SystemTrayIcon class

class Application:
    """Main application class"""
    
    def __init__(self):
        self.args = None
        self.tray_icon = None
        self.data_collector = None
        self._shutting_down = False
        self._shutdown_lock = threading.Lock()
    
    def parse_arguments(self):
        """Parse command line arguments"""
        argParser = argparse.ArgumentParser(description='PiCoMonitor - System monitoring tool for Raspberry Pi Pico')
        argParser.add_argument("-d", "--delay", help="Period for grabbing data (s)", type=float, default=config.DEFAULT_DELAY)
        
        # Port: auto-detected (Raspberry Pi USB id) unless given
        argParser.add_argument("-p", "--port", default=None,
                               help="Serial port of the pico (COMx / /dev/ttyACMx). "
                                    "Default: auto-detect the Pico's USB serial port")
        argParser.add_argument("--list-ports", help="List serial ports (marking the auto-detected one) and exit",
                               action="store_true")
        argParser.add_argument("--no-tray", help="Run without the system tray icon (headless / servers / Raspberry Pi OS Lite)",
                               action="store_true")
        
        argParser.add_argument("--page", choices=sorted(DataCollector.PAGES, key=DataCollector.PAGES.get),
                               help="Show this page on the device (also ends its cycling mode)")
        argParser.add_argument("--cycle", nargs="?", const=config.DEFAULT_CYCLE_S, default=None, type=int, metavar="SECONDS",
                               help=f"Cycle through the pages every SECONDS (default {config.DEFAULT_CYCLE_S}, "
                                    f"{config.MIN_CYCLE_S}-{config.MAX_CYCLE_S}; 0 = off). Both settings are stored by the device")

        # Add logging level option
        argParser.add_argument("--debug", help="Enable debug logging", action="store_true")
        
        self.args = argParser.parse_args()
    
    def configure_logging(self):
        """Configure logging level based on arguments"""
        if self.args.debug:
            logger_setup.set_debug_level()
    
    def validate_arguments(self) -> bool:
        """Validate command line arguments"""
        return ArgumentValidator.validate_all(self.args)
    
    def shutdown(self, reason: str = "requested"):
        """Stop everything, from any thread, any number of times: the data collector (whose thread would
        otherwise keep the process alive after the tray loop ended) and the tray icon."""
        with self._shutdown_lock:
            if self._shutting_down:
                return
            self._shutting_down = True
        logger.info(f"Shutting down ({reason})")
        if self.data_collector:
            self.data_collector.stop()
        if self.tray_icon:
            self.tray_icon.stop_icon()

    def ensure_exit(self, grace: Optional[float] = None):
        """Called once run() returned: give worker threads a moment to finish, then exit anyway.
        A non-daemon thread stuck in a blocking call (a device that stopped answering) would
        otherwise keep the process alive after the user asked to quit."""
        deadline = time.monotonic() + (config.EXIT_GRACE_S if grace is None else grace)
        for t in threading.enumerate():
            if t is not threading.current_thread() and not t.daemon:
                t.join(max(0.0, deadline - time.monotonic()))
        stuck = [t.name for t in threading.enumerate() if t is not threading.current_thread() and not t.daemon]
        if stuck:
            logger.warning(f"Threads still running after shutdown ({', '.join(stuck)}), forcing exit")
            logging.shutdown()
            os._exit(0)

    def setup_signal_handlers(self):
        """Setup signal handlers for graceful shutdown (Python-level; the GTK tray loop gets its own,
        see SystemTrayIcon._install_glib_signal_handlers)"""
        def signal_handler(sig, frame):
            logger.info(f"Received signal {sig}")
            self.shutdown(f"signal {sig}")
        
        signal.signal(signal.SIGINT, signal_handler)
        signal.signal(signal.SIGTERM, signal_handler)
    
    def _arg(self, name: str, kind: type):
        """A command line value of the expected type, else None (older/foreign argument objects)"""
        value = getattr(self.args, name, None)
        return value if isinstance(value, kind) else None

    def initialize_components(self):
        """Initialize application components"""
        # Initialize system tray icon (optional: --no-tray or no tray backend -> headless)
        self.tray_icon = None
        if not getattr(self.args, "no_tray", False):
            try:
                tray = SystemTrayIcon()
                tray.delay = self.args.delay
                tray.page = self._arg("page", str)
                tray.cycle = self._arg("cycle", int)
                tray.initialize()
                tray.on_exit = lambda: self.shutdown("tray Exit")
                tray.on_signal = lambda sig: (logger.info(f"Received signal {sig}"), self.shutdown(f"signal {sig}"))
                self.tray_icon = tray
            except Exception as e:
                logger.warning(f"System tray unavailable ({e}), running headless")
        
        # Initialize data collector
        self.data_collector = DataCollector(self.args.port, self.args.delay)
        if self.tray_icon is not None:
            self.tray_icon.on_delay = self.data_collector.set_delay
            self.tray_icon.on_page = self.data_collector.set_page
            self.tray_icon.on_cycle = self.data_collector.set_cycle
        # UI commands given on the command line (sent to the device whenever it connects)
        if self._arg("page", str):
            self.data_collector.set_page(self.args.page)
        if self._arg("cycle", int) is not None:
            self.data_collector.set_cycle(self.args.cycle)
    
    def run(self):
        """Run the main application"""
        try:
            logger.info(f"Starting PiCoMonitor with delay={self.args.delay}s, port={self.args.port or 'auto-detect'}")
            
            # Start the application
            if self.tray_icon is not None:
                self.tray_icon.run(lambda icon: self.data_collector.work_loop(icon))
                self.shutdown("tray closed")    # the loop ended: make sure nothing keeps running
            else:
                self.data_collector.work_loop(SimpleNamespace(visible=True))   # blocks until Ctrl+C / SIGTERM
            
        except KeyboardInterrupt:
            logger.info("User interrupted application")
            sys.exit(0)
        except Exception as e:
            logger.error(f"Fatal error in main function: {e}")
            sys.exit(1)

def main():
    """Main entry point"""
    try:
        app = Application()
        app.parse_arguments()
        app.configure_logging()

        if app.args.list_ports:
            print(PortDetector.format_ports())
            return
        
        if not app.validate_arguments():
            logger.error("Argument validation failed")
            sys.exit(1)
        
        app.setup_signal_handlers()
        app.initialize_components()
        app.run()
        app.ensure_exit()
        
    except Exception as e:
        logger.error(f"Error in main function: {e}")
        sys.exit(1)
    
if __name__ == "__main__":
    main()
