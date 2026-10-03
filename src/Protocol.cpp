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
