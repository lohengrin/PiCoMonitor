#include "Status.h"

#include <algorithm>
#include <numeric>

namespace {

struct Limits { double warning, critical; };
constexpr Limits kCpu{70, 90};
constexpr Limits kRam{80, 95};
constexpr Limits kTemp{70, 85};
constexpr Limits kDisk{90, 97};

Status classify(double value, const Limits& l)
{
    if (value >= l.critical) return Status::Critical;
    if (value >= l.warning)  return Status::Warning;
    return Status::Ok;
}

Status worst(Status a, Status b)
{
    return static_cast<uint8_t>(a) > static_cast<uint8_t>(b) ? a : b;
}

} // namespace

Status computeStatus(const MonitorData& data)
{
    Status s = Status::Ok;

    if (!data.cpu_percent.empty()) {
        const double avg = std::accumulate(data.cpu_percent.begin(), data.cpu_percent.end(), 0.0)
                           / static_cast<double>(data.cpu_percent.size());
        s = worst(s, classify(avg, kCpu));
    }
    if (data.has_ram)
        s = worst(s, classify(data.ram, kRam));
    if (data.has_temp)
        s = worst(s, classify(data.temp, kTemp));
    for (const auto& d : data.disks)
        if (d.total > 0)
            s = worst(s, classify(100.0 * d.used / d.total, kDisk));

    return s;
}
