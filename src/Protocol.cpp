#include "Protocol.h"
#include "pico_toolset/json_reader.h"

#include <cmath>

bool FrameAssembler::feed(char c)
{
    if (m_done) {           // previous frame was consumed: start fresh
        reset();
    }

    if (m_depth == 0) {
        if (c != '{')
            return false;   // garbage between frames
        m_len = 0;
    }

    if (m_len + 1 >= m_size) { // no room for this byte + NUL
        m_overflows++;
        reset();
        return false;
    }
    m_buf[m_len++] = c;
    m_buf[m_len] = '\0';

    if (m_in_string) {
        if (m_escape)          m_escape = false;
        else if (c == '\\')    m_escape = true;
        else if (c == '"')     m_in_string = false;
        return false;
    }

    if (c == '"') {
        m_in_string = true;
    } else if (c == '{') {
        m_depth++;
    } else if (c == '}') {
        if (--m_depth == 0) {
            m_done = true;
            return true;
        }
    }
    return false;
}

void FrameAssembler::reset()
{
    m_len = 0;
    m_depth = 0;
    m_in_string = false;
    m_escape = false;
    m_done = false;
}

namespace {

using pico_toolset::JsonReader;
using Type = JsonReader::Type;

// A finite number at the reader's position. Anything else (string, null, NaN/Infinity token...)
// is skipped, so a wrongly-typed value never desynchronises the reader.
bool read_number(JsonReader& r, double& out)
{
    if (r.peekType() != Type::Number) { r.skipValue(); return false; }
    return r.parseNumber(out) && std::isfinite(out);
}

// Reads `count` numbers from an array into out[]; false if it is not an array of at least
// `count` numbers (extra elements are ignored).
bool read_numbers(JsonReader& r, double* out, size_t count)
{
    if (r.peekType() != Type::Array) { r.skipValue(); return false; }
    r.beginArray();
    size_t i = 0;
    bool ok = true;
    while (r.nextArrayElement())
    {
        double d;
        const bool good = read_number(r, d);
        if (i < count) { if (good) out[i] = d; else ok = false; }
        ++i;
    }
    return ok && i >= count;
}

// Calls fn(r) for each member of an object, with the key; fn must consume the value.
template <class Fn>
void for_each_member(JsonReader& r, Fn fn)
{
    r.beginObject();
    std::string key;
    while (r.nextObjectMember(key))
        fn(key);
}

// Calls fn(r) for each element of an array
template <class Fn>
void for_each_element(JsonReader& r, Fn fn)
{
    r.beginArray();
    while (r.nextArrayElement())
        fn();
}

bool read_string(JsonReader& r, std::string& out)
{
    if (r.peekType() != Type::String) { r.skipValue(); return false; }
    return r.parseString(out);
}

void decode_gpu(JsonReader& r, MonitorData::Gpu& gpu)
{
    for_each_member(r, [&](const std::string& k) {
        if (k == "n") {
            std::string name;
            if (read_string(r, name))
                gpu.name = name.substr(0, MonitorData::kMaxGpuName);
        }
        else if (k == "l")  gpu.has_load = read_number(r, gpu.load);
        else if (k == "t")  gpu.has_temp = read_number(r, gpu.temp);
        else if (k == "mu") read_number(r, gpu.vram_used_mb);
        else if (k == "mt") read_number(r, gpu.vram_total_mb);
        else                r.skipValue();
    });
    gpu.has_vram = gpu.vram_total_mb > 0.0;
}

void decode_disk(JsonReader& r, MonitorData::DiskData& disk)
{
    for_each_member(r, [&](const std::string& k) {
        if (k == "path")       read_string(r, disk.label);
        else if (k == "total") read_number(r, disk.total);
        else if (k == "used")  read_number(r, disk.used);
        else                   r.skipValue();
    });
}

} // namespace

bool decode_data(const char* json, size_t len, MonitorData& data)
{
    JsonReader r(json, len);
    if (r.peekType() != Type::Object)
        return false;

    for_each_member(r, [&](const std::string& key) {
        if (key == "CPU")
        {
            if (r.peekType() != Type::Array) { r.skipValue(); return; }
            for_each_element(r, [&] {
                double d;
                if (read_number(r, d) && data.cpu_percent.size() < MonitorData::kMaxCores)
                    data.cpu_percent.push_back(d);
            });
        }
        else if (key == "TEMP")  data.has_temp = read_number(r, data.temp);
        else if (key == "RAM")   data.has_ram = read_number(r, data.ram);
        else if (key == "NET")
        {
            double v[2];
            if (read_numbers(r, v, 2)) { data.has_net = true; data.net_down = v[0]; data.net_up = v[1]; }
        }
        else if (key == "IO")
        {
            double v[2];
            if (read_numbers(r, v, 2)) { data.has_io = true; data.io_read = v[0]; data.io_write = v[1]; }
        }
        else if (key == "FREQ")  data.has_freq = read_number(r, data.freq_mhz);
        else if (key == "LOAD")  data.has_load = read_numbers(r, data.load_avg, 3);
        else if (key == "SWAP")  data.has_swap = read_number(r, data.swap);
        else if (key == "UP")
        {
            double up;
            if (read_number(r, up) && up >= 0 && up < 4294967295.0) { data.has_uptime = true; data.uptime_s = static_cast<uint32_t>(up); }
        }
        else if (key == "GPU")
        {
            // an array with one object per GPU (a single object is accepted too)
            auto add = [&] {
                if (r.peekType() == Type::Object && data.gpus.size() < MonitorData::kMaxGpus) {
                    data.gpus.emplace_back();
                    decode_gpu(r, data.gpus.back());
                } else {
                    r.skipValue();
                }
            };
            if (r.peekType() == Type::Array) for_each_element(r, add);
            else add();
        }
        else if (key == "DISKS")
        {
            if (r.peekType() != Type::Array) { r.skipValue(); return; }
            for_each_element(r, [&] {
                if (r.peekType() != Type::Object || data.disks.size() >= MonitorData::kMaxDisks) { r.skipValue(); return; }
                data.disks.emplace_back();
                decode_disk(r, data.disks.back());
            });
        }
        else r.skipValue();
    });

    return r.valid();
}
