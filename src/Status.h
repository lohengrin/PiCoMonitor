#pragma once

// Overall health shown on boards that have a status indicator (the Pico
// Display Pack's RGB LED). Pure C++, host-testable -- see tests/.

#include "Protocol.h"

enum class Status : uint8_t {
    Ok,         // everything below the warning thresholds
    Warning,    // something is high
    Critical,   // something is very high
    NoSignal,   // no data from the host
};

/// @brief Worst-of status for one frame. Warning/critical thresholds:
/// average CPU 70/90 %, RAM 80/95 %, temperature 70/85 degrees, any disk 90/97 %.
/// Values absent from the frame are ignored.
Status computeStatus(const MonitorData& data);
