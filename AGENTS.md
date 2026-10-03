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

- **Pico Display Pack** (Pimoroni) – 240x135, RGB565, includes an RGB LED and 2
  buttons used here. Build with `-DWITH_PICODISPLAY=ON`.
- **CrowPanel 2.8" HMI** (Elecrow) – 320x240, RGB565, XPT2046 touch + uSD card
  (both sharing SPI1 with the display), requires a manual LCD reset on GPIO15.
  This is the default (`-DWITH_CROWPANEL=ON`).

Both boards' display/touch/SD drivers and the Screen/Widget composition layer
come from **`third_party/pico-toolset`**, a git submodule
([lohengrin/Pico-Toolset](https://github.com/lohengrin/Pico-Toolset)) shared
with this author's other Pico projects. See that repo's
`boards/crowpanel_pico_hmi_28.md` for the CrowPanel board's full driver/pin
documentation. Contribute new reusable drivers/presets back there rather than
re-forking them locally.

## Repository layout

```
CMakeLists.txt                 Pico firmware build (selects board + links pico-toolset components)
pico_sdk_import.cmake          locates Raspberry Pi Pico SDK (env PICO_SDK_PATH)
third_party/pico-toolset/      git submodule: shared drivers + Screen/Widget composition
                                (pico_toolset::St7789/Xpt2046Touch/SdCard/Screen/Widget/...)
src/
    PiCoMonitor.cpp            main loop: serial read, JSON decode, widget render, dimming
    Protocol.h / Protocol.cpp  pure-C++ protocol layer (no Pico SDK): MonitorData, FrameAssembler
                               (incremental frame reassembly), decode_data() (picojson, type-safe)
    Com.h / Com.cpp            poll_frame(): non-blocking USB serial read -> FrameAssembler -> decode
tests/                         host-side unit tests for the protocol layer (cmake -S tests ...)
    Input.h                    Corner enum + RepeatFilter (press + auto-repeat), pure C++, host-tested
    Status.h / Status.cpp      computeStatus(): worst-of Ok/Warning/Critical from a frame, pure C++, host-tested
    CrowPanelBoard.*           Elecrow CrowPanel 2.8": pico_toolset St7789+Xpt2046Touch+SdCard
    PicoDisplayBoard.*         Pimoroni Pico Display Pack: pico_toolset St7789 + RgbLed + DebouncedButtons
    Settings.h / Settings.cpp  persistent settings (backlight, page): serialization, SettingsStore (on the toolset's
                               FlashStore) and SettingsSaver (write only after 3 s without change); host-tested
    Pages.h / Pages.cpp        page manager: Overview / Network / System / GPU, owns all widgets, switches the
                               Screen's slots; extra pages become available when the host first sends their data
                               (pure widget logic, host-tested)
    InfoListWidget.*           "label ... value" rows (System page, GPU info)
    OverlayWidget.*            full-screen overlay (Screen::FS slot, drawn last): NO SIGNAL banner + page-name toast
    CPUWidget.*                per-core CPU bars, composed from pico_toolset::BarWidget
    GraphWidget.*              scrolling graph + current value (temp, RAM), wraps pico_toolset::LineGraphWidget
    DiskWidget.*               disk usage bars (pico_toolset::HBarWidget) with the full label drawn inside
                               each bar in black; pages rotate
                               every 4 s when the disks do not all fit (tick())
    picojson.h                 vendored single-header JSON parser (do not modify)
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

Requirements: `pico-sdk` (env `PICO_SDK_PATH`), and the `third_party/pico-toolset`
submodule initialized (`git submodule update --init`). Nothing else: the Pimoroni
libraries are no longer used.

```
git submodule update --init
mkdir build && cd build
cmake -DPICO_BOARD=pico_w -DWITH_CROWPANEL=ON ..    # or -DWITH_PICODISPLAY=ON
make
```

- Default PICO_BOARD is `pico_w`. Pico W links `pico_cyw43_arch_none` (WiFi not
  used yet, only to reach the GPIOs).
- `gpio 15` must be toggled at startup for the CrowPanel LCD reset.
- Build output: `PiCoMonitor.uf2` (plus `.bin/.hex/.elf` via `pico_add_extra_outputs`),
  and a second target `PiCoMonitor_picoboot` producing `PiCoMonitor.picoboot.bin`
  (+ `.uf2`): the same sources linked into the PicoBoot bootloader's app partition
  (`0x10080000`) via `picoboot_set_app_flash_region()`. Options: `PICOBOOT_DIR`
  (default `../PicoBoot`; skipped with a warning if absent), `PICOMONITOR_FLASH_SIZE`
  (total flash, default 2 MiB), `WITH_PICOBOOT` (default ON). Both targets share one
  `picomonitor_configure()` function in `CMakeLists.txt` -- put new link
  libraries/definitions there, not on one target.
- `make install_firmware` builds and copies all `.bin`/`.uf2` to the git-ignored
  `install/`, named `PiCoMonitor-<crowpanel|picodisplay>-<PICO_BOARD>-<build type>[.picoboot].<ext>`.
- `.vscode/` is configured for CMake + cortex-debug.

C++ tests: the protocol layer is unit-tested on the host (ASan/UBSan):
```
cmake -S tests -B build-tests && cmake --build build-tests && ctest --test-dir build-tests --output-on-failure
```
Everything else on the firmware side is verified by building both board options.
Keep `Protocol.*` free of Pico SDK includes so it stays host-testable.

## Host script

Run:
```
python host_script/PiCoMonitor.py [--port <serial-device>] [--delay <sec>] [--no-tray] [--list-ports]
```
Without `--port` the Pico is **auto-detected** (`PortDetector`: Raspberry Pi USB VID `2E8A`,
CDC PID `000A`; ROM-bootloader ids are ignored; product name `PiCoMonitor` ranks first if the
firmware ever reports it) and re-detected on every reconnect, so hot-plug and port-name changes
work. `--list-ports` shows what is found. `--no-tray` (also the automatic fallback when no tray
backend exists) runs headless, which is what Raspberry Pi OS Lite / servers need.
Linux needs serial permission: `sudo usermod -aG dialout $USER` (then log in again).

Host-side structure: the frame is built from a list of `Metric`s (`default_metrics()`; a failing
metric sends its fallback instead of breaking the frame; unknown keys are ignored by the
firmware, but keep the whole frame under the firmware's 1024-byte `FrameAssembler` buffer).
CPU temperature on Linux (`SystemMonitor._get_linux_cpu_temp`) tries `coretemp` (Intel),
`k10temp`/`zenpower` (AMD), `cpu_thermal` (Raspberry Pi) and ACPI before falling back to a GPU
sensor; Windows has no psutil sensors and uses OpenHardwareMonitor if available
(`PICOMONITOR_OHM_DLL` or `host_script/OpenHardwareMonitor/`), otherwise sends no temperature.
Known: 21 older tests (hardware-monitor, disk-label, serial-manager mocks) assume Windows and
fail on Linux; everything else passes (`cd host_script && pytest`).
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

Optional extra keys (omitted by the host when the platform cannot provide them; every one has a
`has_*` flag in `MonitorData`): `NET` `[down, up]` KB/s, `IO` `[read, write]` KB/s, `FREQ` MHz, `LOAD`
`[1, 5, 15]`, `SWAP` %, `UP` seconds, `GPU` `{"n": name, "l": load %, "t": temp, "mu": VRAM used MB,
"mt": VRAM total MB}`. A full frame is ~450 bytes; the firmware's frame buffer is 2048.

`src/Com.cpp` polls the USB serial input (`stdio_usb`, 19200 baud, ignored by
USB CDC) without blocking and feeds `FrameAssembler` (`src/Protocol.h`), which
tolerates any chunking, garbage between frames and braces inside strings, and
drops a half-received frame after 200 ms of silence. `decode_data()` never aborts
on odd JSON: wrongly-typed/null values are ignored, `MonitorData::has_temp`/
`has_ram` say whether those values were present (the main loop only pushes graph
points when set), and core/disk counts are capped (`kMaxCores`/`kMaxDisks`). The baud rate lives in `host_script/PiCoMonitor.py` (`Config.BAUD_RATE`).

## Conventions

- **C++20, C11** (see `CMakeLists.txt` -- raised from C++17 to match
  `third_party/pico-toolset`'s requirement). Firmware uses the Pico SDK and
  `pico_toolset` APIs only (display `St7789`, `RgbLed`, `DebouncedButtons`,
  `Xpt2046Touch`, `SdCard`); there is no Pimoroni dependency anymore.
- Headers declare APIs with Doxygen-style `//!` comments. Keep that style for
  public API.
- Widgets derive from `pico_toolset::Widget` (`draw(DisplayDriver&) const`),
  composing pico-toolset primitives (`BarWidget`/`HBarWidget`/
  `LineGraphWidget`/`TextWidget`). Unlike the old local `Screen`, pico-toolset's
  `Screen` does **not** auto-position widgets into quadrants -- `PiCoMonitor.cpp`
  computes each slot's rect via `screen.slot_rect(Screen::UL/...)` and passes
  explicit pixel coordinates to each widget's constructor. Widgets are owned on
  the stack by `PiCoMonitor.cpp` via `std::unique_ptr`, and registered by raw
  pointer via `screen.set_widget(slot, widget)`.
- Hardware abstraction: no common base class -- `CrowPanelBoard` and
  `PicoDisplayBoard` are unrelated classes selected by CMake
  (`WITH_CROWPANEL`/`WITH_PICODISPLAY`) via a `using Board = ...;` alias in
  `PiCoMonitor.cpp`, both exposing the same duck-typed interface: `driver()`,
  `set_backlight()`, `poll_input()`, `set_status()`, `tick()`, `present()`.
  `CMakeLists.txt` compiles only the selected board's source, and
  `PiCoMonitor.cpp` keeps its `#ifdef`s confined to the board header include +
  `using Board = ...`. Keep both board builds working when touching anything
  platform-related.
- Pages (`Pages.h`): `PiCoMonitor.cpp` calls `pages.update(data)` per frame and
  `pages.tick()` per loop; top-right / bottom-right (press only, `InputEvents::pressed`) switch
  page and show the page name via `OverlayWidget::show_toast`. To add a page: add its widgets and
  a `kCount` entry in `Pages`, gate it with a `m_seen_*` flag set in `update()`, add it to
  `apply()`, and add the host metric as an optional `Metric` in `host_script/PiCoMonitor.py`.
- **Stack**: core 0 has 4 KiB (`PICO_STACK_SIZE=0x1000`, same as PicoBoot; the 2 KiB default overflowed
  once the flash-save path was added on top of `main()`'s frame). Keep big objects off the stack: the
  `Board` is `static` (its display line buffer alone is ~1 KB), `FlashStore` builds records in a member
  buffer. After adding locals to `main()`, re-check its frame (`sub sp` in `objdump -d`, now ~420 bytes).
- Settings persist in the **last 2 flash sectors** (`PICOMONITOR_SETTINGS_SECTORS`,
  pico_toolset `FlashStore`, CRC'd append-only records, erase only every 16 saves). `CMakeLists.txt`
  shortens the firmware's FLASH region by that reserve (standalone: generated
  `pico_flash_region.ld`; PicoBoot image: smaller size passed to `picoboot_set_app_flash_region`),
  so a firmware that grows into it fails at link time. To persist another value: add a field at the END
  of `Settings` and handle the new version in `Settings::deserialize` (old records must stay readable).
  Flash writes halt execution for a few ms (interrupts off): never save per-frame, only via
  `SettingsSaver`. Core1 is not used; `FlashStore` assumes that (see its header).
- Input is the same on both boards: four logical **corners** (`Input.h`).
  `Board::poll_input()` returns the corners that fired this call (press +
  auto-repeat via `RepeatFilter`); `PiCoMonitor.cpp` maps them to actions in one
  place: top-left = backlight up, bottom-left = backlight down (both repeat while
  held), top-right / bottom-right = previous / next page. Pico Display Pack: buttons
  A/B/X/Y are TL/BL/TR/BR. CrowPanel: touching the outer third of both axes in a
  screen corner (`corner_at()`), using the toolset's CrowPanel touch
  calibration (`kElecrowCrowPanelPicoHmi28Calibration`).
- Status: `computeStatus()` (`Status.h`) turns each frame into Ok/Warning/
  Critical (average CPU 70/90 %, RAM 80/95 %, temperature 70/85 degrees, any
  disk 90/97 %); the Pico Display Pack's RGB LED shows it (green/orange/red,
  orange blinking = NO SIGNAL). The CrowPanel has no LED and ignores it.
- The main loop re-renders **only when something changed** (`dirty` flag): a new
  frame, a CPU max marker still falling (`CPUWidget::tick()` returns true while
  moving), or the NO SIGNAL banner toggling. The loop itself still ticks at
  100 Hz to poll USB/buttons. After 3 s (`NoSignalTime`) without a valid frame,
  and only once data was received at least once, `NoSignalWidget` is shown.
  Anything new that animates must return "changed" to the loop the same way.
- Backlight dimming (5 s timeout, gradual fade) lives directly in
  `PiCoMonitor.cpp`'s main loop as two local `uint8_t` variables
  (`backlight`/`target_backlight`) -- it used to live inside the old `Screen`
  base class, which pico-toolset's `Screen` doesn't have room for (it only
  exposes `set_backlight()`).
- Avoid `using namespace` in headers; `using namespace pico_toolset;` is used
  in `.cpp` files.
- The host script is Windows/Linux aware: OpenHardwareMonitor (`.NET`) is used only
  on Windows for GPU temperature; CPU temp is unavailable on Windows.
