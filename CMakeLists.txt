cmake_minimum_required(VERSION 3.20)

# Default to pico_w
if(NOT DEFINED PICO_BOARD)
	SET(PICO_BOARD pico_w)
endif()

# initialize the SDK based on PICO_SDK_PATH
# note: this must happen before project()
include(pico_sdk_import.cmake)

option(WITH_PICODISPLAY "Build for Pimonori LCD" OFF)
option(WITH_CROWPANEL   "Build for Elecrow CrowPanel" ON)

project(PiCoMonitor C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

# Board-specific implementation (compile only the selected one). Both boards'
# display/touch/SD drivers come from the shared third_party/pico-toolset
# submodule -- see that repo's boards/crowpanel_pico_hmi_28.md.
IF(WITH_PICODISPLAY)
    set(BOARD_SRC
        src/PicoDisplayBoard.h
        src/PicoDisplayBoard.cpp
    )
ELSE()
    set(BOARD_SRC
        src/CrowPanelBoard.h
        src/CrowPanelBoard.cpp
    )
ENDIF()

# rest of your project
add_executable(PiCoMonitor
src/picojson.h
src/Com.h
src/Com.cpp
src/CPUWidget.h
src/CPUWidget.cpp
src/GraphWidget.h
src/GraphWidget.cpp
src/DiskWidget.h
src/DiskWidget.cpp
src/PiCoMonitor.cpp
${BOARD_SRC}
)

set(PICO_TOOLSET_BUILD_ST7789  ON  CACHE BOOL "" FORCE)
set(PICO_TOOLSET_BUILD_XPT2046 ON  CACHE BOOL "" FORCE)
set(PICO_TOOLSET_BUILD_SDCARD  ON  CACHE BOOL "" FORCE)
set(PICO_TOOLSET_BUILD_SCREEN  ON  CACHE BOOL "" FORCE)
set(PICO_TOOLSET_BUILD_SSD1306 OFF CACHE BOOL "" FORCE)
set(PICO_TOOLSET_BUILD_ILI9486 OFF CACHE BOOL "" FORCE)
set(PICO_TOOLSET_BUILD_PSRAM   OFF CACHE BOOL "" FORCE)
set(PICO_TOOLSET_BUILD_USB_HID OFF CACHE BOOL "" FORCE)
set(PICO_TOOLSET_BUILD_RESET_BUTTONS OFF CACHE BOOL "" FORCE)
set(PICO_TOOLSET_BUILD_DVI_HDMI OFF CACHE BOOL "" FORCE)
set(PICO_TOOLSET_BUILD_FAULT_HANDLER OFF CACHE BOOL "" FORCE)
include(third_party/pico-toolset/cmake/pico_fatfs.cmake)
add_subdirectory(third_party/pico-toolset)

# Only the Pico Display Pack board needs Pimoroni's RGBLED/Button drivers
# (its display itself now goes through pico_toolset_st7789, same as CrowPanel).
IF(WITH_PICODISPLAY)
    include(pimoroni_pico_import.cmake)
    include(drivers/rgbled/rgbled)
    include(drivers/button/button)
ENDIF()

# Add pico_stdlib library which aggregates commonly used features
target_link_libraries(PiCoMonitor
    pico_stdlib
    pico_toolset_st7789
    pico_toolset_xpt2046
    pico_toolset_sdcard
    pico_toolset_screen
    )

IF(WITH_PICODISPLAY)
    target_link_libraries(PiCoMonitor
        rgbled
        button
    )
    target_compile_definitions(PiCoMonitor PRIVATE WITH_PICODISPLAY)
ELSE()
    target_compile_definitions(PiCoMonitor PRIVATE WITH_CROWPANEL)
ENDIF()

IF (${PICO_BOARD} STREQUAL "pico_w")
    target_link_libraries(PiCoMonitor
        pico_cyw43_arch_none     # we need Wifi to access the GPIO, but we don't need anything else
    )
ENDIF()

pico_enable_stdio_usb(PiCoMonitor 1)
pico_enable_stdio_uart(PiCoMonitor 0)

# create map/bin/hex/uf2 file in addition to ELF.
pico_add_extra_outputs(PiCoMonitor)
