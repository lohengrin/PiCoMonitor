#pragma once

#include "pico/stdlib.h"


// Read json data from serial port
// Return read buffer chars
// Return 0 if invalid
uint16_t get_data(uint8_t *buffer, size_t bufsize);
