PiCoMonitor
-----------

# Introduction

![System running](images/PiCoMonitor_1.jpg)

Rasberry pico (w) based pc monitoring. Use pico display pack from Pimoroni [https://pimoroni.com/displaypack].

Feed by a Python script using psutil + serial.
Transmission of data over USB serial is done by JSON data (see [exemple.json](exemple.json) )

**PiCoMonitor.py** python script support Linux and Windows (need psutil, pyserial, pystray).
CPU Temp is not supported on Windows yet.
This script need to be modified to fit your hardware/software configuration.

# Compilation
Needs:
- [pico-sdk](https://github.com/raspberrypi/pico-sdk)
- The `third_party/pico-toolset` git submodule (display/touch/SD drivers,
  shared with this author's other Pico projects): `git submodule update --init`
- [pimoroni-pico](https://github.com/pimoroni/pimoroni-pico) -- only needed
  for `-DWITH_PICODISPLAY=ON` (RGB LED + buttons)

```
$ git submodule update --init
$ mkdir build
$ cd build
$ cmake -DPICO_BOARD=pico_w ..
$ make
```

Useful options: `-DPICO_BOARD=pico` (plain Pico/RP2040, default `pico_w`),
`-DWITH_PICODISPLAY=ON -DWITH_CROWPANEL=OFF` (Pimoroni Pico Display Pack instead of the
default Elecrow CrowPanel).

## Build outputs
Each build produces two firmware images:

| File | For |
|------|-----|
| `PiCoMonitor.uf2` / `.bin` | Normal firmware, linked at `0x10000000`: copy the `.uf2` to the Pico (BOOTSEL) |
| `PiCoMonitor.picoboot.bin` (+ `.picoboot.uf2`) | Same firmware linked into the [PicoBoot](../PicoBoot) bootloader's application partition (`0x10080000`): put the **`.bin`** on PicoBoot's SD card |

The PicoBoot image needs a PicoBoot checkout (default `../PicoBoot`, override with
`-DPICOBOOT_DIR=...`); if it is not found the build warns and skips it. Other options:
`-DPICOBOOT_FLASH_SIZE=<bytes>` (board flash size, default 2 MiB: CrowPanel, Pico, Pico W) and
`-DWITH_PICOBOOT=OFF` to disable it.

### `install_firmware` target
```
$ make install_firmware
```
builds everything and copies every `.bin` / `.uf2` to `install/` (git-ignored), renamed with
the options of that build (`<display>-<PICO_BOARD>-<build type>`), so several variants can sit
side by side:

```
install/PiCoMonitor-crowpanel-pico-Release.uf2
install/PiCoMonitor-crowpanel-pico-Release.picoboot.bin
install/PiCoMonitor-picodisplay-pico_w-Release.uf2
...
```

# Installation
- Copy uf2 file to the pico or use picotool: 
```
sudo picotool load -f -x PiCoMonitor.uf2 
```
- When launched, start `host_script/PiCoMonitor.py` on the host to monitor.

