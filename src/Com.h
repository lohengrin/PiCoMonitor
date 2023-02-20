#pragma once

#include "pico/stdlib.h"

#include <vector>
#include <string>

/// @brief Struct to store monitoring data received decoded from JSON
struct MonitorData {

    /// @brief Data for disk usage
    struct DiskData {
        std::string label;
        double total;
        double used;
    };

    //! By core CPU usage
    std::vector<double> cpu_percent;
    //! All disks data
    std::vector<DiskData> disks;
    //! RAM used (%)
    double ram;
    //! System temperature
    double temp;
};

/// @brief Read json data from serial port
/// @return read buffer chars, 0 if invalid json/buffer overflow
uint16_t get_data(char *buffer, size_t bufsize);

/// @brief Decode json to MonitorData struct
/// @return false if failed
bool decode_data(char *buffer, size_t bufsize, MonitorData& data);