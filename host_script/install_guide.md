# PiCoMonitor Installation Guide

## Quick Installation

### 1. Install Python Requirements

```bash
pip install -r requirements.txt
```

### 2. Install OpenHardwareMonitor (Windows only)

1. Download OpenHardwareMonitor from: https://openhardwaremonitor.org/
2. Extract the `OpenHardwareMonitorLib.dll` file
3. Place it in the `OpenHardwareMonitor` directory within this project

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
3. **Install OpenHardwareMonitor**:
   - Download from https://openhardwaremonitor.org/
   - Copy `OpenHardwareMonitorLib.dll` to `OpenHardwareMonitor/` directory
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

#### Missing OpenHardwareMonitorLib.dll
- **Solution**: Download from https://openhardwaremonitor.org/ and place in correct directory

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
                       Windows default: COM5
                       Linux default: /dev/ttyACM0
  --debug              Enable debug logging
```

## Verifying Installation

Check that all dependencies are installed:

```bash
pip list | grep -E "(psutil|pyserial|pystray|Pillow|pythonnet)"
```

All modules should be listed with compatible versions.