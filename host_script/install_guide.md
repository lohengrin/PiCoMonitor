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
   python3 -m venv venv
   source venv/bin/activate
   ```
3. **Install requirements**:
   ```bash
   pip install -r requirements.txt
   ```
4. **Install system dependencies**:
   ```bash
   sudo apt-get install libatlas-base-dev
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