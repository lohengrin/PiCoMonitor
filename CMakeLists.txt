cmake_minimum_required(VERSION 3.13)

#Defualt to pico_w
if(NOT DEFINED PICO_BOARD)
	SET(PICO_BOARD pico_w)
endif()

# initialize the SDK based on PICO_SDK_PATH
# note: this must happen before project()
include(pico_sdk_import.cmake)
include(pimoroni_pico_import.cmake)

option(WITH_PICODISPLAY "Build for Pimonori LCD" OFF)
option(WITH_CROWPANEL   "Build for Elecrow CrowPanel" ON)

project(PiCoMonitor C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

# rest of your project
add_executable(PiCoMonitor
src/picojson.h
src/Com.h
src/Com.cpp
src/Screen.h
src/Screen.cpp
src/Widget.h
src/Widget.cpp
src/NullWidget.h
src/NullWidget.cpp
src/CPUWidget.h
src/CPUWidget.cpp
src/GraphWidget.h
src/GraphWidget.cpp
src/DiskWidget.h
src/DiskWidget.cpp
src/PiCoMonitor.cpp
)

include(common/pimoroni_i2c)
include(common/pimoroni_bus)
include(libraries/pico_display/pico_display)
include(libraries/pico_graphics/pico_graphics)
include(drivers/rgbled/rgbled)
include(drivers/button/button)

add_subdirectory(st7789Ex)

# Add pico_stdlib library which aggregates commonly used features
target_link_libraries(PiCoMonitor 
    pico_stdlib
    hardware_spi
    hardware_pwm
    hardware_dma
    pico_display
    pico_graphics
    st7789Ex
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


