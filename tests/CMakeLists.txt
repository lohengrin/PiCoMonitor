# Host-side unit tests for the pure-C++ protocol layer (no Pico SDK needed):
#   cmake -S tests -B build-tests && cmake --build build-tests && ctest --test-dir build-tests --output-on-failure
cmake_minimum_required(VERSION 3.16)
project(PiCoMonitorTests CXX)
set(CMAKE_CXX_STANDARD 20)
enable_testing()

add_executable(test_protocol test_protocol.cpp ../src/Protocol.cpp)
target_include_directories(test_protocol PRIVATE ../src stub)  # stub/: host stand-in for the SDK's pico/assert.h
target_compile_options(test_protocol PRIVATE -Wall -Wextra -fsanitize=address,undefined)
target_link_options(test_protocol PRIVATE -fsanitize=address,undefined)
add_test(NAME protocol COMMAND test_protocol)

add_executable(test_input_status test_input_status.cpp ../src/Status.cpp ../src/Protocol.cpp)
target_include_directories(test_input_status PRIVATE ../src stub)
target_compile_options(test_input_status PRIVATE -Wall -Wextra -fsanitize=address,undefined)
target_link_options(test_input_status PRIVATE -fsanitize=address,undefined)
add_test(NAME input_status COMMAND test_input_status)

# Pages + widgets (header-only toolset screen library, no Pico SDK needed)
set(TOOLSET_SCREEN ../third_party/pico-toolset/libs/screen/include)
set(TOOLSET_IFACES ../third_party/pico-toolset/components/driver_interfaces/include)
add_executable(test_pages test_pages.cpp ../src/Pages.cpp ../src/CPUWidget.cpp ../src/GraphWidget.cpp
               ../src/DiskWidget.cpp ../src/InfoListWidget.cpp ../src/Protocol.cpp)
target_include_directories(test_pages PRIVATE ../src stub ${TOOLSET_SCREEN} ${TOOLSET_IFACES})
target_compile_options(test_pages PRIVATE -Wall -Wextra -fsanitize=address,undefined)
target_link_options(test_pages PRIVATE -fsanitize=address,undefined)
add_test(NAME pages COMMAND test_pages)
