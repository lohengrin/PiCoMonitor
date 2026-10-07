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
        '--add-data=LibreHardwareMonitor/*.dll;LibreHardwareMonitor/',  # LibreHardwareMonitorLib.dll + its dependencies
        '--hidden-import=psutil._pswindows',  # Windows-specific psutil imports
        '--hidden-import=psutil._psposix',   # Linux-specific psutil imports  
        '--hidden-import=pythonnet',         # For .NET interop
        '--hidden-import=clr',              # For .NET CLR
        '--exclude-module=tkinter',         # Exclude unnecessary modules
        '--exclude-module=matplotlib',      # Exclude unnecessary modules
        'PiCoMonitor.py'          # Main script
    ]
    
    # The icon is generated from icon_art.py (committed, regenerate with make_icon.py)
    if not os.path.exists('icon.ico'):
        from make_icon import make_icon
        make_icon()
    
    # Check if LibreHardwareMonitor is present
    if not os.path.exists(os.path.join('LibreHardwareMonitor', 'LibreHardwareMonitorLib.dll')):
        print("Error: LibreHardwareMonitor/LibreHardwareMonitorLib.dll not found")
        print("Extract a LibreHardwareMonitor release (net472 build) into the LibreHardwareMonitor directory")
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