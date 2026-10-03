#include "Protocol.h"
#include "picojson.h"

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

static bool number(const picojson::value& v, double& out)
{
    if (!v.is<double>()) return false;   // also rejects null / strings / bools
    out = v.get<double>();
    return true;
}

// Reads `count` numbers from an array into out[]; false if it is not an array
// of at least `count` numbers.
static bool numbers(const picojson::value& v, double* out, size_t count)
{
    if (!v.is<picojson::array>()) return false;
    const auto& arr = v.get<picojson::array>();
    if (arr.size() < count) return false;
    for (size_t i = 0; i < count; ++i)
        if (!number(arr[i], out[i])) return false;
    return true;
}

static void decode_gpu(const picojson::object& obj, MonitorData::Gpu& gpu)
{
    for (const auto& kv : obj)
    {
        const std::string& k = kv.first;
        const picojson::value& v = kv.second;
        if (k == "n" && v.is<std::string>())
            gpu.name = v.get<std::string>().substr(0, MonitorData::kMaxGpuName);
        else if (k == "l")
            gpu.has_load = number(v, gpu.load);
        else if (k == "t")
            gpu.has_temp = number(v, gpu.temp);
        else if (k == "mu")
            number(v, gpu.vram_used_mb);
        else if (k == "mt")
            number(v, gpu.vram_total_mb);
    }
    gpu.has_vram = gpu.vram_total_mb > 0.0;
}

bool decode_data(const char* json, size_t len, MonitorData& data)
{
    picojson::value v;
    std::string err;
    picojson::parse(v, json, json + len, &err);
    if (!err.empty() || !v.is<picojson::object>())
        return false;

    const auto& obj = v.get<picojson::object>();
    for (const auto& kv : obj)
    {
        const std::string& key = kv.first;
        const picojson::value& val = kv.second;

        if (key == "CPU" && val.is<picojson::array>())
        {
            for (const auto& c : val.get<picojson::array>())
            {
                double d;
                if (data.cpu_percent.size() < MonitorData::kMaxCores && number(c, d))
                    data.cpu_percent.push_back(d);
            }
        }
        else if (key == "TEMP")
        {
            data.has_temp = number(val, data.temp);
        }
        else if (key == "RAM")
        {
            data.has_ram = number(val, data.ram);
        }
        else if (key == "NET")
        {
            double v[2];
            if (numbers(val, v, 2)) { data.has_net = true; data.net_down = v[0]; data.net_up = v[1]; }
        }
        else if (key == "IO")
        {
            double v[2];
            if (numbers(val, v, 2)) { data.has_io = true; data.io_read = v[0]; data.io_write = v[1]; }
        }
        else if (key == "FREQ")
        {
            data.has_freq = number(val, data.freq_mhz);
        }
        else if (key == "LOAD")
        {
            data.has_load = numbers(val, data.load_avg, 3);
        }
        else if (key == "SWAP")
        {
            data.has_swap = number(val, data.swap);
        }
        else if (key == "UP")
        {
            double up;
            if (number(val, up) && up >= 0 && up < 4294967295.0) { data.has_uptime = true; data.uptime_s = static_cast<uint32_t>(up); }
        }
        else if (key == "GPU")
        {
            // an array with one object per GPU (a single object is accepted too)
            auto add = [&](const picojson::value& g) {
                if (g.is<picojson::object>() && data.gpus.size() < MonitorData::kMaxGpus) {
                    data.gpus.emplace_back();
                    decode_gpu(g.get<picojson::object>(), data.gpus.back());
                }
            };
            if (val.is<picojson::array>())
                for (const auto& g : val.get<picojson::array>()) add(g);
            else
                add(val);
        }
        else if (key == "DISKS" && val.is<picojson::array>())
        {
            for (const auto& item : val.get<picojson::array>())
            {
                if (data.disks.size() >= MonitorData::kMaxDisks) break;
                if (!item.is<picojson::object>()) continue;

                MonitorData::DiskData disk;
                for (const auto& d : item.get<picojson::object>())
                {
                    if (d.first == "path" && d.second.is<std::string>())
                        disk.label = d.second.get<std::string>();
                    else if (d.first == "total")
                        number(d.second, disk.total);
                    else if (d.first == "used")
                        number(d.second, disk.used);
                }
                data.disks.push_back(std::move(disk));
            }
        }
    }

    return true;
}
