# PiCoMonitor Distribution Guide

## Build Successful! 🎉

The PiCoMonitor EXE has been successfully built and is ready for distribution.

## Distribution Files

### Main Executable
- **Location**: `dist/PiCoMonitor.exe`
- **Size**: ~28.4 MB
- **Type**: Standalone Windows executable

### Build Artifacts
- **Build directory**: `build/` - Contains PyInstaller build files
- **Spec file**: `PiCoMonitor.spec` - PyInstaller specification file
- **Warnings**: `build/PiCoMonitor/warn-PiCoMonitor.txt` - Build warnings

## Distribution Package

### Option 1: Single EXE Distribution
The `dist/PiCoMonitor.exe` file is a standalone executable that includes all required dependencies.

### Option 2: Complete Distribution Package
For a more robust distribution, create a package with:

```
PiCoMonitor_Distribution/
├── PiCoMonitor.exe          # Main executable
├── OpenHardwareMonitor/      # Required DLLs
│   ├── OpenHardwareMonitorLib.dll
│   ├── Aga.Controls.dll
│   ├── OxyPlot.dll
│   └── OxyPlot.WindowsForms.dll
├── README.md                 # User instructions
├── requirements.txt          # Python requirements (for source)
└── install_guide.md          # Installation instructions
```

## Deployment Instructions

### For End Users (EXE only)
1. Copy `dist/PiCoMonitor.exe` to any Windows machine
2. Run the EXE directly (no installation required)
3. The application will start with default settings

### For Developers (Full package)
1. Copy the entire `PiCoMonitor_Distribution` folder
2. Ensure all DLL files are in the `OpenHardwareMonitor` subdirectory
3. Run `PiCoMonitor.exe` from the distribution folder

## Command Line Usage

```bash
# Basic usage
PiCoMonitor.exe

# With custom serial port
PiCoMonitor.exe -p COM3

# With custom delay
PiCoMonitor.exe -d 1.0

# With debug logging
PiCoMonitor.exe --debug
```

## Troubleshooting

### Common Issues

1. **Missing DLL errors**: Ensure all files from `OpenHardwareMonitor/` are distributed with the EXE
2. **Serial port not found**: Use `-p` flag to specify correct COM port
3. **Permission issues**: Run as administrator if serial port access is denied
4. **Antivirus warnings**: The EXE may be flagged as unknown - add exception

### Verification

To verify the build works:

```bash
# Test the EXE
./dist/PiCoMonitor.exe --help

# Check for any missing dependencies
./dist/PiCoMonitor.exe -p COM1 -d 0.5
```

## Build Information

- **Build Date**: 2026-06-08
- **Python Version**: 3.12.10
- **PyInstaller Version**: 6.20.0
- **Platform**: Windows 11
- **Architecture**: 64-bit

## Notes

- The EXE includes all Python dependencies (psutil, pystray, Pillow, etc.)
- OpenHardwareMonitor DLLs are bundled for GPU temperature monitoring
- The application runs as a system tray icon with no console window
- Default serial port: COM5 (Windows) or /dev/ttyACM0 (Linux)
- Default data collection interval: 0.5 seconds

## Future Improvements

For future builds, consider:
1. Adding a custom icon (create `icon.ico`)
2. Code signing the EXE for security
3. Creating an installer package (MSI)
4. Adding automatic updates functionality