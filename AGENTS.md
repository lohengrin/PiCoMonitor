# AGENTS.md

Guidance for AI agents working in this repository.

## Project overview

PiCoMonitor is a PC-monitoring system made of two parts:

- **Embedded firmware** (C++17) running on a Raspberry Pi Pico / Pico W. It renders
  CPU/temperature/RAM/disk widgets on a small LCD and receives monitoring data over
  USB serial as JSON.
- **Host script** (Python 3.8+) that collects system data with `psutil`
  (plus OpenHardwareMonitor on Windows) and sends it to the Pico over serial.

Two display boards are supported, selected at **compile time**:

- **Pico Display Pack** (Pimoroni) – 240x135, RGB565, includes an RGB LED and 4
  buttons. Build with `-DWITH_PICODISPLAY=ON`.
- **CrowPanel 2.8" HMI** (Elecrow) – 320x240, RGB332, no LED/buttons, requires a
  manual LCD reset on GPIO15. This is the default (`-DWITH_CROWPANEL=ON`).

## Repository layout

```
CMakeLists.txt                 Pico firmware build (selects board + screen source)
pico_sdk_import.cmake          locates Raspberry Pi Pico SDK (env PICO_SDK_PATH)
pimoroni_pico_import.cmake     locates Pimoroni Pico libraries (env PIMORONI_PICO_PATH)
src/
    PiCoMonitor.cpp            main loop: serial read, JSON decode, widget render, dimming
    Com.h / Com.cpp            USB serial reading + JSON decoding (picojson)
    Screen.h / Screen.cpp      abstract Screen base: PicoGraphics buffer, widget slots,
                               drawing primitives, backlight state + dimming hooks
    ScreenPicoDisplay.*        Pimoroni Pico Display Pack: ST7789 (PIMORONI) + RGB LED + buttons
    ScreenCrowPanel.*          Elecrow CrowPanel 2.8": ST7789 (ELECROW) + GPIO15 LCD reset
    Widget.h / Widget.cpp      abstract base class for all widgets
    NullWidget.*               placeholder/empty widget
    CPUWidget.*                per-core CPU bars with max cursor
    GraphWidget.*              scrolling graph + current value (temp, RAM)
    DiskWidget.*               disk usage bars
    picojson.h                 vendored single-header JSON parser (do not modify)
st7789Ex/                      fork of the Pimoroni ST7789 driver, extended with an
                               ELECROW variant (320x240 SPI, board-reset handling)
host_script/
    PiCoMonitor.py             host monitoring daemon (CLI args, tray icon, logging)
    build_exe.py / *.spec      PyInstaller packaging
    tests/                     pytest suite (mirrors every host module)
    pytest.ini                 host test configuration
    requirements*.txt          host Python dependencies
    docs/                      distribution & error-handling guides
    exemple.json               sample JSON frame sent to the Pico
images/                        screenshots
.vscode/                       build/debug config (cortex-debug for the Pico)
```

## Build (firmware)

Requirements: `pico-sdk`, `pimoroni-pico` (see README.md). Both are located via
`PICO_SDK_PATH` / `PIMORONI_PICO_PATH` environment variables, or fetched from git.

```
mkdir build && cd build
cmake -DPICO_BOARD=pico_w -DWITH_CROWPANEL=ON ..    # or -DWITH_PICODISPLAY=ON
make
```

- Default PICO_BOARD is `pico_w`. Pico W links `pico_cyw43_arch_none` (WiFi not
  used yet, only to reach the GPIOs).
- `gpio 15` must be toggled at startup for the CrowPanel LCD reset.
- Build output: `PiCoMonitor.uf2` (plus `.bin/.hex/.elf` via `pico_add_extra_outputs`).
- `.vscode/` is configured for CMake + cortex-debug.

There is no automated test or lint step for the C++ side; correctness is verified by
building for both board options.

## Host script

Run:
```
python host_script/PiCoMonitor.py --port <serial-device> [--delay <sec>]
```
Command-line flags are documented with `python host_script/PiCoMonitor.py --help`.

Tests (from `host_script/`):
```
pytest                # uses host_script/pytest.ini
```

Packaging: `python host_script/build_exe.py` builds a Windows executable via
PyInstaller (see `host_script/docs/DISTRIBUTION_GUIDE.md`).

## Serial protocol

The host sends one JSON object per frame (see `host_script/exemple.json`):

```json
{
  "CPU":   [0.0, 0.0, ...],      // per-core usage %
  "TEMP":  32.0,                  // CPU temperature °C
  "RAM":   51.1,                  // used %
  "DISKS": [ {"path": "/", "total": 982.83, "used": 218.18}, ... ]  // GB
}
```

`src/Com.cpp` (re)discovers frames by brace nesting on the USB serial input
(`stdio_usb`, 19200 baud) and decodes with picojson into `MonitorData`
(`src/Com.h`). The baud rate lives in `host_script/PiCoMonitor.py` (`Config.BAUD_RATE`).

## Conventions

- **C++17, C11** (see `CMakeLists.txt`). Firmware uses the Pico SDK and Pimoroni
  `pico_graphics`/`pico_display` APIs.
- Headers declare APIs with Doxygen-style `//!` comments. Keep that style for
  public API.
- Widgets derive from `Widget` (abstract `init()` / `draw()`), are positioned via
  `Screen::Slot` (UL/UR/BL/BR/FS), and draw with a `PicoGraphics` object handed
  to them by the screen. Widgets are owned on the stack by `PiCoMonitor.cpp` via
  `std::unique_ptr`, and registered by raw pointer on the `Screen`.
- Hardware abstraction: `Screen` is an abstract base class owning
  `std::unique_ptr<PicoGraphics>`. Each board derives from it
  (`ScreenPicoDisplay`, `ScreenCrowPanel`) and implements `update()` and
  `apply_backlight()`. Board-specific peripherals live in the subclass and hook
  into the main loop via `onFrameBegin()` (e.g. buttons adjust backlight) and
  `onDataReceived()` (e.g. RGB LED cycling). `CMakeLists.txt` compiles only the
  selected board's screen source, and `PiCoMonitor.cpp` keeps one `#ifdef`
  confined to the `createScreen()` factory. Keep both board builds working when
  touching anything platform-related.
- `set_backlight()`/`set_target_backlight()` track applied vs. user backlight
  levels; the main loop dims the applied level below the target when no data
  arrives (5 s timeout).
- Avoid `using namespace` in headers; `using namespace pimoroni;` is used in `.cpp`
  files.
- `st7789Ex` is a local fork of the upstream Pimoroni ST7789 driver. `screen_type`
  (`PIMORONI` / `ELECROW`) changes init timing and MADCTL configuration. Both
  boards use this fork via `PiCoMonitor::ST7789EX`.
- The host script is Windows/Linux aware: OpenHardwareMonitor (`.NET`) is used only
  on Windows for GPU temperature; CPU temp is unavailable on Windows.
