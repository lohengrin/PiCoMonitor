#pragma once

#include "pico/stdlib.h"

#include <vector>

struct MonitorData {
    std::vector<double> cpu_percent;
    double ram;
    double temp;
};

// Read json data from serial port
// Return read buffer chars
// Return 0 if invalid
uint16_t get_data(char *buffer, size_t bufsize);

bool decode_data(char *buffer, size_t bufsize, MonitorData& data);