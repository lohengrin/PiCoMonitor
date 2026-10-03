#include "Com.h"

#include "pico/stdlib.h"

namespace {

constexpr size_t kFrameBufferSize = 1024;
//! A frame is written by the host in one go: if bytes stop arriving this long
//! mid-frame, the rest was lost -- drop it and resynchronise.
constexpr int64_t kFrameTimeoutUs = 200000;

char s_buffer[kFrameBufferSize];
FrameAssembler s_assembler(s_buffer, kFrameBufferSize);
absolute_time_t s_last_byte;

} // namespace

bool poll_frame(MonitorData& data)
{
    const absolute_time_t now = get_absolute_time();
    if (s_assembler.in_frame() && absolute_time_diff_us(s_last_byte, now) > kFrameTimeoutUs)
        s_assembler.reset();

    for (;;)
    {
        int c = getchar_timeout_us(0);
        if (c == PICO_ERROR_TIMEOUT)
            return false;

        s_last_byte = get_absolute_time();
        if (s_assembler.feed(static_cast<char>(c)))
        {
            data = MonitorData();
            return decode_data(s_assembler.buffer(), s_assembler.length(), data);
        }
    }
}
