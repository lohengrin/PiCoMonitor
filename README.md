# PiCoMonitor

![System running](images/PiCoMonitor_1.jpg)

A small PC monitor built on a Raspberry Pi Pico / Pico W. A Python script on the host collects CPU, temperature,
RAM, disks, network, GPU... with `psutil` and sends it over USB serial as JSON
(see [exemple.json](host_script/exemple.json)); the firmware draws it on a small display.

## Supported hardware

One firmware, three display boards, chosen at build time:

| Board | Display | Controls | CMake option |
|-------|---------|----------|--------------|
| Elecrow CrowPanel 2.8" HMI (default) | 320x240 colour, touch | touch the screen corners | `-DWITH_CROWPANEL=ON` |
| [Pimoroni Pico Display Pack](https://pimoroni.com/displaypack) | 240x135 colour, RGB status LED | buttons A / B / X / Y | `-DWITH_PICODISPLAY=ON -DWITH_CROWPANEL=OFF` |
| Pico W + Waveshare Pico-ePaper-2.13 V4 | 250x122 black & white e-paper | BOOTSEL button | `-DWITH_EPD=ON` |

Display, touch and SD drivers come from the `third_party/pico-toolset` submodule, shared with the author's other Pico projects.

## Build

Needs the [pico-sdk](https://github.com/raspberrypi/pico-sdk) (`PICO_SDK_PATH`) and the submodule.

```
git submodule update --init
cmake -S . -B build -DPICO_BOARD=pico_w -DWITH_CROWPANEL=ON    # or one of the other boards above
cmake --build build
```

`-DPICO_BOARD=pico` builds for a plain Pico (default `pico_w`). The result is `build/PiCoMonitor.uf2`.

`make install_firmware` builds and copies every `.bin` / `.uf2` to the git-ignored `install/`, named after the
build options (`PiCoMonitor-<crowpanel|picodisplay|epd>-<PICO_BOARD>-<build type>.uf2`), so several variants can sit side by side.

## Install

Hold BOOTSEL while plugging the Pico and copy the `.uf2` to it, or:
```
sudo picotool load -f -x PiCoMonitor.uf2
```
Then start the host script (below).

## Host script

`host_script/PiCoMonitor.py` (Windows, Ubuntu, Raspberry Pi OS; needs `psutil`, `pyserial`, `pystray`) finds the Pico by its USB id:
```
python host_script/PiCoMonitor.py                # auto-detect the port
python host_script/PiCoMonitor.py --list-ports   # show serial ports / what was detected
python host_script/PiCoMonitor.py -p /dev/ttyACM0  # force a port (COM3 on Windows)
python host_script/PiCoMonitor.py --no-tray      # headless (servers, Raspberry Pi OS Lite)
python host_script/PiCoMonitor.py --page gpu     # show a page on the device (overview, network, system, gpu)
python host_script/PiCoMonitor.py --cycle 20     # page cycling mode, a new page every 20 s (--cycle alone: 30 s, 0: off)
```
The tray menu has the same choices (page, cycling period) plus the frame period (0.25 / 0.5 / 1 / 2 s, default 0.5 s).
- Linux: your user needs serial access: `sudo usermod -aG dialout $USER` (log in again).
- Raspberry Pi OS (bookworm/trixie, Wayland): the panel's tray is AppIndicator-only, so the tray icon
  needs `sudo apt install python3-gi gir1.2-ayatanaappindicator3-0.1` **and** a venv that can see the
  system packages (`python3 -m venv --system-site-packages venv`), or `/usr/bin/python3` with apt's
  `python3-pystray`. With an isolated venv pystray silently picks an XEmbed backend and no icon shows;
  the script warns about it, and `--no-tray` runs headless. The panel also only renders icons found in
  the icon theme, so the script installs its own there on first run (a running panel may need
  `pkill wf-panel-pi` or a logout to pick it up).
- CPU temperature works out of the box on Linux (Intel, AMD, Raspberry Pi). On Windows it needs
  LibreHardwareMonitor (`host_script/LibreHardwareMonitor/` or `PICOMONITOR_LHM_DLL`) and administrator rights for the CPU
  sensor (otherwise the GPU temperature is sent).
- Load: the script reads sensors only as often as needed (slow values such as disks every 30 s, temperature every 2 s) and
  sends compact frames, about 0.7 % of one core at the default period.
- GPU: NVIDIA via `nvidia-ml-py` or the driver's `nvidia-smi`, AMD on Linux; Intel GPUs are not supported.

## Pages and controls

Up to four pages; the extra ones appear the first time the host sends the matching data.

| Page | Content |
|------|---------|
| Overview | RAM and temperature graphs, per-core CPU bars, disk usage bars |
| Network | network and disk throughput graphs |
| System | CPU frequency, cores, load average, swap, uptime |
| GPU | load, temperature and VRAM graphs; with two GPUs, one column each |

| Corner | Action |
|--------|--------|
| top-left / bottom-left | backlight up / down (hold to repeat) |
| top-right / bottom-right | previous / next page |
| top-right / bottom-right, held 1 s | page cycling mode |

Corners are the buttons A, B, X, Y on the Pico Display Pack and the outer third of the screen corners on the CrowPanel.
The e-paper has neither backlight nor corners: **BOOTSEL** shows the next page. The page name appears briefly after a switch.

**Page cycling mode:** the device shows the pages one after the other, every 30 s by default, starting from the current page.
Hold a page button (BOOTSEL on the e-paper) for 1 s to start it; any manual page change (button or host `--page`) ends it.
The host can start it too (`--cycle`, tray).

Board specifics:
- **Pico Display Pack:** the RGB LED shows the overall status (green / orange / red; blinking orange = no signal).
- **E-paper:** colours are drawn as black on white. The panel is refreshed at most every 2 s (a page switch refreshes at once)
  with a full clear every 30 minutes against ghosting.

## Settings

Backlight level, current page and cycling mode survive reboots, whether set with the buttons or from the host (in cycling
mode the stored page is the one it started from). They are stored in the last two flash sectors and written 30 s after the
last change, so holding a button doesn't wear the flash.

## PicoBoot

Each build also produces `PiCoMonitor.picoboot.bin` (+ `.picoboot.uf2`): the same firmware linked into the application
partition (`0x10080000`) of the [PicoBoot](../PicoBoot) bootloader. Put the **`.bin`** on PicoBoot's SD card. The saved
settings are kept when PicoBoot reloads the app.

The image needs a PicoBoot checkout (default `../PicoBoot`, override with `-DPICOBOOT_DIR=...`; skipped with a warning if
absent). Options: `-DWITH_PICOBOOT=OFF` to disable it, `-DPICOMONITOR_FLASH_SIZE=<bytes>` for the board's flash size (default 2 MiB).
`install_firmware` copies it as `PiCoMonitor-<board>-<PICO_BOARD>-<build type>.picoboot.bin`.
