# -*- mode: python ; coding: utf-8 -*-


a = Analysis(
    ['PiCoMonitor.py'],
    pathex=[],
    binaries=[],
    datas=[('OpenHardwareMonitor/OpenHardwareMonitorLib.dll', 'OpenHardwareMonitor/'), ('OpenHardwareMonitor/*.dll', 'OpenHardwareMonitor/')],
    hiddenimports=['psutil._pswindows', 'psutil._psposix', 'pythonnet', 'clr'],
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludes=['tkinter', 'matplotlib'],
    noarchive=False,
    optimize=0,
)
pyz = PYZ(a.pure)

exe = EXE(
    pyz,
    a.scripts,
    a.binaries,
    a.datas,
    [],
    name='PiCoMonitor',
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=True,
    upx_exclude=[],
    runtime_tmpdir=None,
    console=False,
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
)
