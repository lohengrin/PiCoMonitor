#!/usr/bin/env python3
"""
PyInstaller build script for PiCoMonitor
Creates a standalone EXE distribution
"""

import os
import sys
from PyInstaller.__main__ import run as pyinstaller_run

def build_exe():
    """Build the PiCoMonitor EXE using PyInstaller"""
    
    # Define the build parameters
    build_params = [
        '--onefile',              # Create a single EXE file
        '--windowed',             # No console window (GUI application)
        '--icon=icon.ico',        # Use custom icon if available
        '--name=PiCoMonitor',     # Application name
        '--clean',               # Clean PyInstaller cache
        '--noconfirm',           # Don't ask for confirmation
        '--log-level=INFO',       # Log level
        '--add-data=OpenHardwareMonitor/OpenHardwareMonitorLib.dll;OpenHardwareMonitor/',  # Include DLL
        '--add-data=OpenHardwareMonitor/*.dll;OpenHardwareMonitor/',  # Include any other DLLs
        '--hidden-import=psutil._pswindows',  # Windows-specific psutil imports
        '--hidden-import=psutil._psposix',   # Linux-specific psutil imports  
        '--hidden-import=pythonnet',         # For .NET interop
        '--hidden-import=clr',              # For .NET CLR
        '--exclude-module=tkinter',         # Exclude unnecessary modules
        '--exclude-module=matplotlib',      # Exclude unnecessary modules
        'PiCoMonitor.py'          # Main script
    ]
    
    # Check if we have a custom icon
    if not os.path.exists('icon.ico'):
        print("Warning: icon.ico not found, using default icon")
        build_params.remove('--icon=icon.ico')
    
    # Check if OpenHardwareMonitor directory exists
    if not os.path.exists('OpenHardwareMonitor'):
        print("Error: OpenHardwareMonitor directory not found")
        print("Please ensure OpenHardwareMonitorLib.dll is in the OpenHardwareMonitor directory")
        return False
    
        # Version file was removed due to format issues
    
    try:
        print("Building PiCoMonitor EXE...")
        print(f"Command: pyinstaller {' '.join(build_params)}")
        
        # Run PyInstaller
        pyinstaller_run(build_params)
        
        print("\nBuild completed successfully!")
        print("EXE file location: dist/PiCoMonitor.exe")
        
        return True
        
    except Exception as e:
        print(f"Build failed: {e}")
        return False

if __name__ == "__main__":
    success = build_exe()
    sys.exit(0 if success else 1)