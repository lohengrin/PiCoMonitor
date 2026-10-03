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
from typing import Callable, Dict, List, Any, Optional, Tuple
from dataclasses import dataclass
from contextlib import contextmanager

from PIL import Image, ImageDraw


def find_ohm_dll() -> Optional[str]:
    """Locate OpenHardwareMonitorLib.dll (Windows only): $PICOMONITOR_OHM_DLL,
    then OpenHardwareMonitor/ next to the script (or inside the PyInstaller bundle)."""
    candidates = []
    env = os.environ.get("PICOMONITOR_OHM_DLL")
    if env:
        candidates.append(env)
    for base in (getattr(sys, "_MEIPASS", None), os.path.dirname(os.path.abspath(__file__))):
        if base:
            candidates.append(os.path.join(base, "OpenHardwareMonitor", "OpenHardwareMonitorLib.dll"))
    for path in candidates:
        if os.path.isfile(path):
            return path
    return None


# OpenHardwareMonitor is Windows-only (.NET); optional: without it (or without
# pythonnet) the GPU temperature is simply unavailable.
Computer = None
if sys.platform == "win32":
    try:
        import clr  # the pythonnet module (Windows only)
        _ohm_dll = find_ohm_dll()
        if _ohm_dll is None:
            raise FileNotFoundError("OpenHardwareMonitorLib.dll not found "
                                    "(set PICOMONITOR_OHM_DLL or put it in host_script/OpenHardwareMonitor/)")
        clr.AddReference(_ohm_dll)
        from OpenHardwareMonitor.Hardware import Computer
    except Exception as _e:
        Computer = None
        print(f"OpenHardwareMonitor unavailable, no GPU temperature: {_e}", file=sys.stderr)

# Configuration constants
@dataclass
class Config:
    DEFAULT_DELAY: float = 0.5
    MAX_RETRY_ATTEMPTS: int = 3
    SERIAL_TIMEOUT: int = 1
    BAUD_RATE: int = 19200
    LOG_FILE: str = "pico_monitor.log"
    MAX_LOG_SIZE: int = 1048576  # 1MB
    LOG_BACKUP_COUNT: int = 3

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
                timeout=self.timeout
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
    """Class to handle hardware monitoring functionality"""
    
    def __init__(self):
        self.computer = None
        if sys.platform == "win32":
            self._initialize()
    
    def _initialize(self):
        """Initialize hardware monitoring with error handling (Windows only)"""
        try:
            self.computer = Computer()
            self.computer.GPUEnabled = True
            self.computer.Open()
            logger.info("Hardware monitoring initialized successfully")
        except Exception as e:
            logger.error(f"Failed to initialize hardware monitoring: {e}")
            self.computer = None
    
    def get_gpu_temperature(self) -> Optional[float]:
        """Get GPU temperature using OpenHardwareMonitor (Windows only)"""
        if sys.platform != "win32" or self.computer is None:
            logger.warning("Hardware monitoring not available on this platform")
            return None
        
        try:
            self.computer.Hardware[0].Update()
            for sensor in self.computer.Hardware[0].Sensors:
                if "/temperature" in str(sensor.Identifier):
                    return sensor.get_Value()
            logger.warning("No temperature sensor found in OpenHardwareMonitor")
            return None
        except Exception as e:
            logger.warning(f"Failed to get GPU temperature: {e}")
            return None
    
    def is_available(self) -> bool:
        """Check if hardware monitoring is available"""
        return sys.platform == "win32" and self.computer is not None

# Initialize hardware monitor
hardware_monitor = HardwareMonitor()

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
        * Windows → drive letter only (e.g. "C")
        * Linux   → strip leading "/mnt/" if present
        """
        if sys.platform.startswith("linux"):
            if path.startswith("/mnt/"):
                return path.replace("/mnt/", "")
            return path
        # Windows – typical mount point looks like "C:\\" or "D:\\"
        # Extract the first character (drive letter) and return it upper‑cased
        if len(path) >= 2 and path[1] == ":":
            return path[0].upper()
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
                return hardware_monitor.get_gpu_temperature()
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
    def get_memory_usage() -> Optional[float]:
        """Get memory usage percentage"""
        try:
            mem_percent = psutil.virtual_memory().percent
            return mem_percent
        except (psutil.Error, MemoryError) as e:
            logger.error(f"Failed to get memory usage: {e}")
            return None

# Remove old functions as they're now in SystemMonitor class

class SystemTrayIcon:
    """Class to manage system tray icon and menu"""
    
    def __init__(self, title: str = 'PiCoMonitor - System Monitoring Tool'):
        self.title = title
        self.icon = None
        self.contflag = True
        self.delay = config.DEFAULT_DELAY
        self.current_delay = self.delay
    
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
        return Menu(
            MenuItem('0.25 s', lambda i: self.set_delay(0.25), checked=lambda i: self.get_delay(0.25)),
            MenuItem('0.50 s', lambda i: self.set_delay(0.5), checked=lambda i: self.get_delay(0.5)),
            MenuItem('1.00 s', lambda i: self.set_delay(1.0), checked=lambda i: self.get_delay(1.0)),
            Menu.SEPARATOR,
            MenuItem('Exit', lambda: self.exit_action())
        )
    
    def set_delay(self, delay_value: float):
        """Set the data collection delay"""
        self.delay = delay_value
        logger.info(f"Delay set to {delay_value}s")
    
    def get_delay(self, delay_value: float) -> bool:
        """Check if current delay matches the given value"""
        return self.delay == delay_value
    
    def exit_action(self):
        """Handle application exit"""
        try:
            logger.info("User requested exit")
            self.contflag = False
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
        self.icon = pystray.Icon('PiCoMonitor', icon=self.create_image(64, 64, 'blue', 'white'))
        self.icon.menu = self.create_menu()
        self.icon.title = self.title
    
    def run(self, work_function):
        """Run the system tray icon with the given work function"""
        if self.icon:
            self.icon.run(work_function)

@dataclass
class Metric:
    """One entry of the frame sent to the Pico. `collect(collector)` returns the
    value; if it raises, `fallback` is sent instead so one broken sensor never
    affects the others. Keys the firmware does not know are ignored by it."""
    key: str
    collect: Callable[['DataCollector'], Any]
    fallback: Any = None


def default_metrics() -> List[Metric]:
    # Looked up through SystemMonitor at call time (patchable in tests)
    return [
        Metric('CPU', lambda dc: SystemMonitor.get_cpu_usage(dc.delay), []),   # blocks `delay` s: paces the loop
        Metric('TEMP', lambda dc: SystemMonitor.get_cpu_temperature(), None),
        Metric('RAM', lambda dc: SystemMonitor.get_memory_usage(), None),
        Metric('DISKS', lambda dc: SystemMonitor.get_disk_usage(), []),
    ]


class DataCollector:
    """Class to handle data collection and serial communication"""
    
    def __init__(self, port: Optional[str], initial_delay: float, metrics: Optional[List[Metric]] = None):
        # port None = auto-detect (re-evaluated at every (re)connection attempt)
        self.port = port
        self.delay = initial_delay
        self.contflag = True
        self.serial_manager = None
        self.metrics = metrics if metrics is not None else default_metrics()
        self._wake = threading.Event()
    
    def collect_system_data(self) -> Dict[str, Any]:
        """Collect system monitoring data"""
        data = {}
        for metric in self.metrics:
            try:
                data[metric.key] = metric.collect(self)
            except Exception as e:
                logger.error(f"Metric {metric.key} failed: {e}")
                data[metric.key] = metric.fallback
        return data
    
    def send_data_via_serial(self, data: Dict[str, Any], serial_conn: serial.Serial):
        """Send data via serial connection"""
        try:
            serialized_data = json.dumps(data)
            if not serial_conn or not serial_conn.is_open:
                logger.warning("Serial port not open for writing")
                return False
            # Use UTF-8 to be safe with any characters
            serial_conn.write(serialized_data.encode('utf-8'))
            serial_conn.flush()  # Ensure data is sent immediately
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
        while self.contflag:
            # Auto-detect on every attempt: the device may be plugged in later or
            # change its port name
            port = self.port or PortDetector.find()
            if port is None:
                logger.info("No Pico serial port found, waiting for the device...")
                self._wake.wait(min(retry_delay, 5))
                continue
            try:
                # Attempt to open serial port with context manager
                with SerialPortManager(port, config.BAUD_RATE, config.SERIAL_TIMEOUT) as serial_conn:
                    logger.info(f'Connected to serial port: {serial_conn.name}')
                    # Reset back‑off after a successful connection
                    retry_delay = 2
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
                            # Continue collecting; do not break unless contflag cleared
                            continue
            except SerialCommunicationError as e:
                logger.warning(f'Serial port connection failed: {e}')
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
    
    def setup_signal_handlers(self):
        """Setup signal handlers for graceful shutdown"""
        def signal_handler(sig, frame):
            logger.info(f"Received signal {sig}, shutting down gracefully")
            if self.tray_icon:
                self.tray_icon.exit_action()
            if self.data_collector:
                self.data_collector.stop()
        
        signal.signal(signal.SIGINT, signal_handler)
        signal.signal(signal.SIGTERM, signal_handler)
    
    def initialize_components(self):
        """Initialize application components"""
        # Initialize system tray icon (optional: --no-tray or no tray backend -> headless)
        self.tray_icon = None
        if not getattr(self.args, "no_tray", False):
            try:
                tray = SystemTrayIcon()
                tray.initialize()
                self.tray_icon = tray
            except Exception as e:
                logger.warning(f"System tray unavailable ({e}), running headless")
        
        # Initialize data collector
        self.data_collector = DataCollector(self.args.port, self.args.delay)
    
    def run(self):
        """Run the main application"""
        try:
            logger.info(f"Starting PiCoMonitor with delay={self.args.delay}s, port={self.args.port or 'auto-detect'}")
            
            # Start the application
            if self.tray_icon is not None:
                self.tray_icon.run(lambda icon: self.data_collector.work_loop(icon))
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
        
    except Exception as e:
        logger.error(f"Error in main function: {e}")
        sys.exit(1)
    
if __name__ == "__main__":
    main()
