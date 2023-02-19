#pragma once

#include "pico/stdlib.h"

#include <vector>
#include <string>

struct MonitorData {
    struct DiskData {
        std::string label;
        double total;
        double used;
    };

    std::vector<double> cpu_percent;
    std::vector<DiskData> disks;
    double ram;
    double temp;
};

// Read json data from serial port
// Return read buffer chars
// Return 0 if invalid
uint16_t get_data(char *buffer, size_t bufsize);

bool decode_data(char *buffer, size_t bufsize, MonitorData& data);