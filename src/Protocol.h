#pragma once

// Host <-> firmware protocol: pure C++ (no Pico SDK), so it is unit-testable on
// the host -- see tests/.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

/// @brief Monitoring data decoded from one JSON frame
struct MonitorData {

    /// @brief Data for disk usage
    struct DiskData {
        std::string label;
        double total = 0.0;
        double used = 0.0;
    };

    //! Limits applied while decoding (extra entries are ignored)
    static constexpr size_t kMaxCores = 64;
    static constexpr size_t kMaxDisks = 8;

    //! By core CPU usage (%)
    std::vector<double> cpu_percent;
    //! All disks data
    std::vector<DiskData> disks;
    //! RAM used (%) -- only meaningful if has_ram
    double ram = 0.0;
    //! System temperature -- only meaningful if has_temp
    double temp = 0.0;
    //! False when the key is absent, null, or not a number in the frame
    bool has_ram = false;
    bool has_temp = false;
};

/// @brief Incremental frame reassembler: feed it bytes as they arrive, it
/// returns true when one complete top-level JSON object is in the buffer.
/// Tolerates arbitrary chunking, garbage between frames, and braces inside
/// strings; resynchronises on the next '{' after an overflow or reset().
class FrameAssembler {
public:
    FrameAssembler(char* buffer, size_t size) : m_buf(buffer), m_size(size) {}

    //! Feed one byte. True when a complete frame is now in buffer()
    //! (NUL-terminated, length() bytes). The next feed() starts a new frame.
    bool feed(char c);

    //! Drop any partial frame (e.g. after a timeout)
    void reset();

    bool in_frame() const { return m_depth > 0; }
    size_t length() const { return m_len; }
    const char* buffer() const { return m_buf; }
    //! Frames abandoned because they exceeded the buffer
    uint32_t overflows() const { return m_overflows; }

private:
    char* m_buf;
    size_t m_size;
    size_t m_len = 0;
    int m_depth = 0;
    bool m_in_string = false;
    bool m_escape = false;
    bool m_done = false;
    uint32_t m_overflows = 0;
};

/// @brief Decode one JSON frame. Never aborts on malformed/odd-typed content:
/// unknown keys and wrongly-typed values are ignored (the matching has_* flag
/// stays false).
/// @return false if the frame is not a JSON object
bool decode_data(const char* json, size_t len, MonitorData& data);
