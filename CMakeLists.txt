cmake_minimum_required(VERSION 3.13)

#Defualt to pico_w
if(NOT DEFINED PICO_BOARD)
	SET(PICO_BOARD pico_w)
endif()

# initialize the SDK based on PICO_SDK_PATH
# note: this must happen before project()
include(pico_sdk_import.cmake)

project(PiCoMonitor)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

# rest of your project
add_executable(PiCoMonitor
    picojson.h
    Com.h
    Com.cpp
    Screen.h
    Screen.cpp
	PiCoMonitor.cpp
   )

# Add pico_stdlib library which aggregates commonly used features
target_link_libraries(PiCoMonitor 
    pico_stdlib
    pico_cyw43_arch_none     # we need Wifi to access the GPIO, but we don't need anything else
)

pico_enable_stdio_usb(PiCoMonitor 1)
pico_enable_stdio_uart(PiCoMonitor 0)

# create map/bin/hex/uf2 file in addition to ELF.
pico_add_extra_outputs(PiCoMonitor)
