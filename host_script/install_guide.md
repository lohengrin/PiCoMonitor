# PiCoMonitor Installation Guide

## Quick Installation

### 1. Install Python Requirements

```bash
pip install -r requirements.txt
```

### 2. Install LibreHardwareMonitor (Windows only)

1. Download a LibreHardwareMonitor release (.NET Framework build) from: https://github.com/LibreHardwareMonitor/LibreHardwareMonitor/releases
2. Extract the whole archive into the `LibreHardwareMonitor` directory within this project
   (`LibreHardwareMonitorLib.dll` needs the other DLLs next to it), or point
   `PICOMONITOR_LHM_DLL` to `LibreHardwareMonitorLib.dll`
3. Run PiCoMonitor as administrator to get the CPU temperature (without it, the GPU
   temperature is sent instead)

### 3. Install System Dependencies (Linux)

```bash
# For Ubuntu/Debian
sudo apt-get update
sudo apt-get install python3-pip python3-dev libatlas-base-dev

# Install serial port access (add user to dialout group)
sudo usermod -a -G dialout $USER
```

#### Tray icon on Raspberry Pi OS (bookworm/trixie, Wayland)

Raspberry Pi OS uses Wayland (`labwc`/Wayfire) and its panel only provides an
**AppIndicator** system tray, so the tray icon needs PyGObject and an ayatana
indicator library (both are *system* packages, not pip packages):

```bash
sudo apt-get install python3-gi gir1.2-ayatanaappindicator3-0.1
```

pystray must be able to `import gi`. A virtual environment created with a plain
`python3 -m venv venv` hides the system packages, so pystray falls back to its
Xorg/XEmbed backend, which has no tray under Wayland: the icon silently never
appears. Create the venv with system site-packages instead:

```bash
python3 -m venv --system-site-packages venv
```

The panel can also only load icons that live in the icon theme, so on startup the
script installs its tray icon there itself
(`~/.local/share/icons/hicolor/<size>/apps/picomonitor.png`). A panel that was
already running when the icon first appeared may need a restart
(`pkill wf-panel-pi`) or a logout to pick it up.

## Detailed Installation

### Windows Installation

1. **Install Python 3.8+** from https://www.python.org/downloads/
2. **Install requirements**:
   ```bash
   pip install -r requirements.txt
   ```
3. **Install LibreHardwareMonitor**:
   - Download from https://github.com/LibreHardwareMonitor/LibreHardwareMonitor/releases
   - Extract the archive into the `LibreHardwareMonitor/` directory
4. **Run the application**:
   ```bash
   python PiCoMonitor.py
   ```

### Linux Installation

1. **Install Python 3.8+**:
   ```bash
   sudo apt-get update
   sudo apt-get install python3 python3-pip python3-venv
   ```
2. **Create virtual environment (recommended)**:
   ```bash
   python3 -m venv --system-site-packages venv
   source venv/bin/activate
   ```
   `--system-site-packages` matters for the tray icon: PyGObject (`python3-gi`) is
   only available as a system package, and an isolated venv hides it, which makes
   pystray fall back to an XEmbed backend that has no tray under Wayland.
3. **Install requirements**:
   ```bash
   pip install -r requirements.txt
   ```
4. **Install system dependencies**:
   ```bash
   sudo apt-get install libatlas-base-dev
   # Raspberry Pi OS tray icon (Wayland): AppIndicator backend
   sudo apt-get install python3-gi gir1.2-ayatanaappindicator3-0.1
   sudo usermod -a -G dialout $USER
   ```
5. **Log out and back in** for group changes to take effect
6. **Run the application**:
   ```bash
   python PiCoMonitor.py
   ```

## Troubleshooting

### Common Issues and Solutions

#### Serial Port Not Found
- **Windows**: Check Device Manager for COM port number
- **Linux**: Check `/dev/ttyACM*` or `/dev/ttyUSB*`
- **Solution**: Use `-p` flag to specify correct port:
  ```bash
  python PiCoMonitor.py -p COM3        # Windows
  python PiCoMonitor.py -p /dev/ttyACM0  # Linux
  ```

#### Permission Denied (Linux)
- **Solution**: Add user to dialout group and reboot:
  ```bash
  sudo usermod -a -G dialout $USER
  ```

#### Tray icon missing on Raspberry Pi OS (Wayland)
- **Symptom**: The script runs and collects data, but no icon appears in the panel; a log line may say
  `Tray icon: pystray selected the Xorg backend, which has no system tray under Wayland`.
- **Cause**: pystray's AppIndicator backend needs PyGObject (`import gi`), which a plain
  (non-`--system-site-packages`) venv hides, so pystray selects its XEmbed backend. Under Wayland there
  is no XEmbed system tray.
- **Solution**: Install the system packages and recreate the venv with system site-packages (or use
  `/usr/bin/python3` with `apt`'s `python3-pystray`):
  ```bash
  sudo apt-get install python3-gi gir1.2-ayatanaappindicator3-0.1
  python3 -m venv --system-site-packages venv
  source venv/bin/activate && pip install -r requirements.txt
  ```
  Or run headless with `--no-tray`.
- **Note on the icon itself**: the panel can only load icons that exist in the icon theme, so the
  script installs its own icon there on startup (`~/.local/share/icons/hicolor/<size>/apps/picomonitor.png`,
  plus the theme cache). A panel that was already running when the icon was first installed may need a
  restart (`pkill wf-panel-pi`, respawned automatically) or a logout to pick it up.

#### Missing LibreHardwareMonitorLib.dll
- **Solution**: Download from https://github.com/LibreHardwareMonitor/LibreHardwareMonitor/releases and extract into `LibreHardwareMonitor/`

#### No CPU temperature on Windows (GPU temperature shown instead)
- **Solution**: LibreHardwareMonitor needs administrator rights to read the CPU sensors: run PiCoMonitor as administrator

#### Python Module Not Found
- **Solution**: Install missing module:
  ```bash
  pip install missing-module-name
  ```

## Running with Debug Logging

To enable detailed debug logging:

```bash
python PiCoMonitor.py --debug
```

## Command Line Options

```
Usage: PiCoMonitor.py [OPTIONS]

Options:
  -d, --delay FLOAT    Period for grabbing data (seconds), default: 0.5
  -p, --port TEXT      Serial port of pico
                       Default: auto-detect the Pico's USB serial port
  --list-ports         List serial ports (marking the auto-detected one) and exit
  --no-tray            Run without the tray icon (headless: servers, Raspberry Pi OS Lite)
  --debug              Enable debug logging
```

## Verifying Installation

Check that all dependencies are installed:

```bash
pip list | grep -E "(psutil|pyserial|pystray|Pillow|pythonnet)"
```

All modules should be listed with compatible versions.