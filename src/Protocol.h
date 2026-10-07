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
    static constexpr size_t kMaxGpuName = 24;
    static constexpr size_t kMaxGpus = 2;

    //! By core CPU usage (%)
    std::vector<double> cpu_percent;
    //! All disks data -- only meaningful if has_disks (the host sends it every ~30 s, not in every frame)
    std::vector<DiskData> disks;
    bool has_disks = false;
    //! RAM used (%) -- only meaningful if has_ram
    double ram = 0.0;
    //! System temperature -- only meaningful if has_temp
    double temp = 0.0;
    //! False when the key is absent, null, or not a number in the frame
    bool has_ram = false;
    bool has_temp = false;

    // Optional extras (shown on the extra pages); each has_* is false when the
    // host did not send it (platform cannot provide it, or first sample).
    bool   has_net = false;      //! network throughput, KB/s
    double net_down = 0.0, net_up = 0.0;
    bool   has_io = false;       //! disk throughput, KB/s
    double io_read = 0.0, io_write = 0.0;
    bool   has_freq = false;     //! CPU frequency, MHz
    double freq_mhz = 0.0;
    bool   has_load = false;     //! load average over 1/5/15 minutes
    double load_avg[3] = {0.0, 0.0, 0.0};
    bool   has_swap = false;     //! swap usage, %
    double swap = 0.0;
    bool   has_uptime = false;   //! seconds since boot
    uint32_t uptime_s = 0;

    //! Host commands for the UI (sent only when the host user changed them)
    bool has_page = false;       //! select this Pages::Id (a manual change: ends the cycling mode)
    uint8_t page = 0;
    bool has_cycle = false;      //! cycling mode period in seconds, 0 = off
    uint8_t cycle_s = 0;

    /// @brief One GPU (NVIDIA / AMD); the host lists every GPU it finds, none on e.g. a Raspberry Pi
    struct Gpu {
        std::string name;
        bool has_load = false;  double load = 0.0;      //! %
        bool has_temp = false;  double temp = 0.0;      //! degrees
        bool has_vram = false;  double vram_used_mb = 0.0, vram_total_mb = 0.0;
    };
    //! The first kMaxGpus GPUs (a discrete + an integrated one, say); empty if none
    std::vector<Gpu> gpus;
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
