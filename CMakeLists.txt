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

# Firmware sources, shared by the standalone image and the PicoBoot image
set(PICOMONITOR_SRC
src/picojson.h
src/Protocol.h
src/Protocol.cpp
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

add_executable(PiCoMonitor ${PICOMONITOR_SRC})

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

# Optional second image for the PicoBoot bootloader (a raw .bin linked into
# its application partition): <name>.picoboot.bin. Needs a PicoBoot checkout,
# located via -DPICOBOOT_DIR=... (default: sibling ../PicoBoot).
set(PICOBOOT_DIR "${CMAKE_CURRENT_LIST_DIR}/../PicoBoot" CACHE PATH "PicoBoot checkout")
set(PICOBOOT_FLASH_SIZE 2097152 CACHE STRING "Total flash size (bytes) of the target board")
option(WITH_PICOBOOT "Also build PiCoMonitor.picoboot.bin for PicoBoot" ON)

# Common link libraries, definitions and stdio settings for a firmware target
function(picomonitor_configure TARGET)
    target_link_libraries(${TARGET}
        pico_stdlib
        pico_toolset_st7789
        pico_toolset_xpt2046
        pico_toolset_sdcard
        pico_toolset_screen
        )

    IF(WITH_PICODISPLAY)
        target_link_libraries(${TARGET} rgbled button)
        target_compile_definitions(${TARGET} PRIVATE WITH_PICODISPLAY)
    ELSE()
        target_compile_definitions(${TARGET} PRIVATE WITH_CROWPANEL)
    ENDIF()

    IF (${PICO_BOARD} STREQUAL "pico_w")
        # we need Wifi to access the GPIO, but we don't need anything else
        target_link_libraries(${TARGET} pico_cyw43_arch_none)
    ENDIF()

    pico_enable_stdio_usb(${TARGET} 1)
    pico_enable_stdio_uart(${TARGET} 0)

    # create map/bin/hex/uf2 file in addition to ELF.
    pico_add_extra_outputs(${TARGET})
endfunction()

picomonitor_configure(PiCoMonitor)

IF(WITH_PICOBOOT)
    IF(EXISTS ${PICOBOOT_DIR}/cmake/picoboot_app_linker.cmake)
        add_executable(PiCoMonitor_picoboot ${PICOMONITOR_SRC})
        set_target_properties(PiCoMonitor_picoboot PROPERTIES OUTPUT_NAME PiCoMonitor.picoboot)
        include(${PICOBOOT_DIR}/cmake/picoboot_app_linker.cmake)
        picoboot_set_app_flash_region(PiCoMonitor_picoboot ${PICOBOOT_FLASH_SIZE})
        picomonitor_configure(PiCoMonitor_picoboot)   # -> PiCoMonitor.picoboot.bin
    ELSE()
        message(WARNING "PicoBoot not found at ${PICOBOOT_DIR} (set PICOBOOT_DIR): skipping PiCoMonitor.picoboot.bin")
    ENDIF()
ENDIF()

# install_firmware: copy every .bin/.uf2 to <source>/install/, renaming with
# the options used for this build (display, PICO_BOARD, build type), e.g.
#   PiCoMonitor-crowpanel-pico_w-Release.uf2
#   PiCoMonitor-crowpanel-pico_w-Release.picoboot.bin
IF(WITH_PICODISPLAY)
    set(FW_DISPLAY picodisplay)
ELSE()
    set(FW_DISPLAY crowpanel)
ENDIF()
set(FW_SUFFIX "${FW_DISPLAY}-${PICO_BOARD}-${CMAKE_BUILD_TYPE}")
set(FW_INSTALL_DIR ${CMAKE_CURRENT_LIST_DIR}/install)

set(FW_TARGETS PiCoMonitor)
IF(TARGET PiCoMonitor_picoboot)
    list(APPEND FW_TARGETS PiCoMonitor_picoboot)
ENDIF()

add_custom_target(install_firmware
    COMMAND ${CMAKE_COMMAND} -E make_directory ${FW_INSTALL_DIR}
    COMMENT "Copying firmware to ${FW_INSTALL_DIR} (suffix ${FW_SUFFIX})"
    VERBATIM)
foreach(T ${FW_TARGETS})
    get_target_property(OUT_NAME ${T} OUTPUT_NAME)
    IF(NOT OUT_NAME)
        set(OUT_NAME ${T})
    ENDIF()
    # OUT_NAME is "PiCoMonitor" or "PiCoMonitor.picoboot": insert the suffix
    # after the base name, keeping ".picoboot" before the extension.
    string(REPLACE ".picoboot" "" BASE ${OUT_NAME})
    string(REPLACE "${BASE}" "" KIND ${OUT_NAME})
    add_dependencies(install_firmware ${T})
    foreach(EXT bin uf2)
        add_custom_command(TARGET install_firmware POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy
                $<TARGET_FILE_DIR:${T}>/${OUT_NAME}.${EXT}
                ${FW_INSTALL_DIR}/${BASE}-${FW_SUFFIX}${KIND}.${EXT}
            VERBATIM)
    endforeach()
endforeach()
