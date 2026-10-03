#pragma once

#include "Protocol.h"

/// @brief Poll the USB serial input (non-blocking) and reassemble frames.
/// Call once per main-loop iteration.
/// @return true when a complete, valid frame was decoded into data
bool poll_frame(MonitorData& data);
