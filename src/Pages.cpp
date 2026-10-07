#include "Pages.h"

#include <cstdio>

using namespace pico_toolset;

namespace {

// KB/s (decimal) -> "340 KB/s", "1.2 MB/s", "3.4 GB/s"
void fmt_rate(double kbps, char* buf, size_t n)
{
    if (kbps < 10)           snprintf(buf, n, "%.1f KB/s", kbps);
    else if (kbps < 1000)    snprintf(buf, n, "%.0f KB/s", kbps);
    else if (kbps < 1e6)     snprintf(buf, n, "%.1f MB/s", kbps / 1000.0);
    else                     snprintf(buf, n, "%.1f GB/s", kbps / 1e6);
}

void fmt_percent(double v, char* buf, size_t n) { snprintf(buf, n, "%.0f%%", v); }

void fmt_degrees(double v, char* buf, size_t n) { snprintf(buf, n, "%.0f\xC2\xB0" "C", v); }

std::string format(const char* fmt, double a)
{
    char b[32];
    snprintf(b, sizeof b, fmt, a);
    return b;
}

std::string format_uptime(uint32_t s)
{
    char b[32];
    const uint32_t days = s / 86400, h = (s / 3600) % 24, m = (s / 60) % 60, sec = s % 60;
    if (days > 0) snprintf(b, sizeof b, "%ud %02u:%02u", static_cast<unsigned>(days), static_cast<unsigned>(h), static_cast<unsigned>(m));
    else          snprintf(b, sizeof b, "%02u:%02u:%02u", static_cast<unsigned>(h), static_cast<unsigned>(m), static_cast<unsigned>(sec));
    return b;
}

const char* const kNames[] = {"Overview", "Network", "System", "GPU"};

} // namespace

Pages::Pages(Screen& screen) : m_screen(screen)
{
    const Screen::SlotRect ul = screen.slot_rect(Screen::UL);
    const Screen::SlotRect ur = screen.slot_rect(Screen::UR);
    const Screen::SlotRect bl = screen.slot_rect(Screen::BL);
    const Screen::SlotRect br = screen.slot_rect(Screen::BR);
    const Screen::SlotRect fs = screen.slot_rect(Screen::FS);

    // Overview (the original layout)
    m_cpu.reset(new CPUWidget(bl.x, bl.y, bl.w, bl.h));
    m_temp.reset(new GraphWidget(ur.x, ur.y, ur.w, ur.h, 100, Color::from_rgb888(10, 10, 255), "\xC2\xB0" "C"));
    m_ram.reset(new GraphWidget(ul.x, ul.y, ul.w, ul.h, 100, Color::from_rgb888(10, 255, 10), "RAM"));
    m_disks.reset(new DiskWidget(br.x, br.y, br.w, br.h));

    // Network
    auto rate_graph = [](const Screen::SlotRect& r, const char* title, Color c, double min_scale) {
        std::unique_ptr<GraphWidget> g(new GraphWidget(r.x, r.y, r.w, r.h, min_scale, c, title));
        g->setAutoScale(min_scale);
        g->setTitle(title);
        g->setFormatter(fmt_rate);
        return g;
    };
    m_net_down = rate_graph(ul, "NET DOWN", Color::from_rgb888(60, 170, 255), 50);
    m_net_up   = rate_graph(ur, "NET UP", Color::from_rgb888(255, 170, 40), 50);
    m_io_read  = rate_graph(bl, "DISK READ", Color::from_rgb888(70, 220, 160), 500);
    m_io_write = rate_graph(br, "DISK WRITE", Color::from_rgb888(230, 90, 200), 500);

    // System: one list over the whole screen
    m_system.reset(new InfoListWidget(fs.x, fs.y, fs.w, fs.h, "System"));

    // GPU
    auto percent_graph = [](const Screen::SlotRect& r, const char* title, Color c, GraphWidget::ValueFormatter f) {
        std::unique_ptr<GraphWidget> g(new GraphWidget(r.x, r.y, r.w, r.h, 100, c, title));
        g->setTitle(title);
        g->setFormatter(f);
        return g;
    };
    m_gpu_load = percent_graph(ul, "GPU LOAD", Color::from_rgb888(255, 130, 40), fmt_percent);
    m_gpu_temp = percent_graph(ur, "GPU TEMP", Color::from_rgb888(10, 10, 255), fmt_degrees);
    m_gpu_vram = percent_graph(bl, "VRAM", Color::from_rgb888(200, 90, 255), fmt_percent);
    m_gpu_info.reset(new InfoListWidget(br.x, br.y, br.w, br.h, "GPU"));

    // two GPUs: columns, load graph on top and info below
    m_gpu2_load[0] = percent_graph(ul, "GPU1 LOAD", Color::from_rgb888(255, 130, 40), fmt_percent);
    m_gpu2_load[1] = percent_graph(ur, "GPU2 LOAD", Color::from_rgb888(200, 90, 255), fmt_percent);
    m_gpu2_info[0].reset(new InfoListWidget(bl.x, bl.y, bl.w, bl.h, "GPU1"));
    m_gpu2_info[1].reset(new InfoListWidget(br.x, br.y, br.w, br.h, "GPU2"));

    apply();
}

bool Pages::available(Id id) const
{
    switch (id) {
        case Overview: return true;
        case Network:  return m_seen_net;
        case System:   return m_seen_system;
        case Gpu:      return m_gpu_count > 0;
        default:       return false;
    }
}

void Pages::show(Id id)
{
    if (id != m_current) {
        m_current = id;
        apply();
    }
}

void Pages::select(Id id)
{
    if (id < 0 || id >= kCount)
        return;
    if (available(id)) {
        m_pending = -1;
        m_base = id;
        show(id);
    } else {
        m_pending = id;
    }
}

void Pages::set_preferred(Id id)
{
    if (id < 0 || id >= kCount)
        return;
    m_cycle_s = 0;
    select(id);
}

void Pages::restore(Id id, uint8_t cycle_s)
{
    if (id < 0 || id >= kCount)
        id = Overview;
    select(id);
    if (cycle_s > 0) {
        m_cycle_s = m_cycle_period = cycle_s;
        m_cycle_arm = true;
    }
}

void Pages::set_cycle(uint8_t seconds)
{
    m_cycle_s = seconds;
    if (seconds == 0)
        return;
    m_cycle_period = seconds;
    m_cycle_arm = true;
    if (m_pending < 0)
        m_base = m_current;     // it starts from the page showing now
}

std::string Pages::cycle_toast() const
{
    return "Cycling " + std::to_string(m_cycle_s) + " s";
}

int Pages::next_available(int from, int dir) const
{
    int id = from;
    for (int i = 0; i < kCount; ++i) {
        id = (id + dir + kCount) % kCount;
        if (available(static_cast<Id>(id)))
            return id;
    }
    return from;
}

void Pages::step(int dir)
{
    m_pending = -1;     // the user chose: forget the restored page
    m_cycle_s = 0;      // ... and the cycling mode ends
    show(static_cast<Id>(next_available(m_current, dir)));
    m_base = m_current;
}

bool Pages::advance(uint32_t now_ms)
{
    if (m_cycle_s == 0 || m_pending >= 0)
        return false;
    if (m_cycle_arm) {
        m_cycle_arm = false;
        m_cycle_deadline_ms = now_ms + m_cycle_s * 1000u;
        return false;
    }
    if (static_cast<int32_t>(now_ms - m_cycle_deadline_ms) < 0)
        return false;
    m_cycle_deadline_ms = now_ms + m_cycle_s * 1000u;
    const Id before = m_current;
    show(static_cast<Id>(next_available(m_current, +1)));   // m_base stays: the page the cycle started from
    return m_current != before;
}

void Pages::apply()
{
    Widget* ul = nullptr; Widget* ur = nullptr; Widget* bl = nullptr; Widget* br = nullptr;
    switch (m_current) {
        case Overview: ul = m_ram.get();      ur = m_temp.get();      bl = m_cpu.get();       br = m_disks.get();   break;
        case Network:  ul = m_net_down.get(); ur = m_net_up.get();    bl = m_io_read.get();   br = m_io_write.get(); break;
        case System:   ul = m_system.get(); break;                    // covers the whole screen
        case Gpu:
            if (m_gpu_count >= 2) { ul = m_gpu2_load[0].get(); ur = m_gpu2_load[1].get();
                                    bl = m_gpu2_info[0].get(); br = m_gpu2_info[1].get(); }
            else                  { ul = m_gpu_load.get(); ur = m_gpu_temp.get();
                                    bl = m_gpu_vram.get(); br = m_gpu_info.get(); }
            break;
        default: break;
    }
    m_screen.set_widget(Screen::UL, ul);
    m_screen.set_widget(Screen::UR, ur);
    m_screen.set_widget(Screen::BL, bl);
    m_screen.set_widget(Screen::BR, br);
}

std::string Pages::toast() const
{
    int pos = 0, total = 0;
    for (int i = 0; i < kCount; ++i) {
        if (!available(static_cast<Id>(i))) continue;
        ++total;
        if (i == m_current) pos = total;
    }
    char b[40];
    snprintf(b, sizeof b, "%d/%d %s", pos, total, kNames[m_current]);
    return b;
}

bool Pages::tick()
{
    const bool cpu_moved = m_cpu->tick();
    const bool disks_moved = m_disks->tick();
    return m_current == Overview && (cpu_moved || disks_moved);
}

void Pages::update(const MonitorData& d)
{
    // Overview
    m_cpu->setValues(d.cpu_percent);
    if (d.has_temp) m_temp->pushValue(d.temp);
    if (d.has_ram)  m_ram->pushValue(d.ram);
    m_disks->setValues(d.disks);

    // Network
    if (d.has_net) { m_net_down->pushValue(d.net_down); m_net_up->pushValue(d.net_up); }
    if (d.has_io)  { m_io_read->pushValue(d.io_read);   m_io_write->pushValue(d.io_write); }
    if (d.has_net || d.has_io) m_seen_net = true;

    // System
    std::vector<InfoListWidget::Row> rows;
    if (d.has_freq)
        rows.emplace_back("CPU freq", d.freq_mhz < 1000 ? format("%.0f MHz", d.freq_mhz) : format("%.2f GHz", d.freq_mhz / 1000.0));
    if (!d.cpu_percent.empty())
        rows.emplace_back("Cores", std::to_string(d.cpu_percent.size()));
    if (d.has_load) {
        char b[40];
        snprintf(b, sizeof b, "%.2f %.2f %.2f", d.load_avg[0], d.load_avg[1], d.load_avg[2]);
        rows.emplace_back("Load", b);
    }
    if (d.has_swap)
        rows.emplace_back("Swap", format("%.0f %%", d.swap));
    if (d.has_uptime)
        rows.emplace_back("Uptime", format_uptime(d.uptime_s));
    if (d.has_freq || d.has_load || d.has_swap || d.has_uptime) {
        m_system->setRows(std::move(rows));
        m_seen_system = true;
    }

    // GPU (one page: the single-GPU layout, or two columns when there are two GPUs)
    if (!d.gpus.empty()) {
        const size_t count = d.gpus.size();
        const bool layout_changes = count > m_gpu_count && (count >= 2) != (m_gpu_count >= 2);
        if (count > m_gpu_count) m_gpu_count = count;

        for (size_t i = 0; i < count; ++i) {
            const MonitorData::Gpu& g = d.gpus[i];

            std::vector<InfoListWidget::Row> info;
            if (!g.name.empty())
                info.emplace_back("GPU", g.name);
            if (g.has_temp)
                info.emplace_back("Temp", format("%.0f\xC2\xB0" "C", g.temp));
            if (g.has_vram) {
                char b[40];
                snprintf(b, sizeof b, "%.1f/%.1f GB", g.vram_used_mb / 1024.0, g.vram_total_mb / 1024.0);
                info.emplace_back("VRAM", b);
            }

            if (g.has_load) m_gpu2_load[i]->pushValue(g.load);
            m_gpu2_info[i]->setRows(info);

            if (i == 0) {                      // the single-GPU layout shows the first GPU only
                if (g.has_load) m_gpu_load->pushValue(g.load);
                if (g.has_temp) m_gpu_temp->pushValue(g.temp);
                if (g.has_vram) m_gpu_vram->pushValue(100.0 * g.vram_used_mb / g.vram_total_mb);
                std::vector<InfoListWidget::Row> single;
                if (!g.name.empty()) single.emplace_back("GPU", g.name);
                if (g.has_vram) single.emplace_back("VRAM", info.back().second);
                m_gpu_info->setRows(std::move(single));
            }
        }
        if (layout_changes && m_current == Gpu)
            apply();                           // a second GPU appeared while this page is shown
    }

    // A restored page that was not available at boot: show it once it is
    if (m_pending >= 0 && available(static_cast<Id>(m_pending))) {
        select(static_cast<Id>(m_pending));
        m_cycle_arm = true;     // a restored cycling mode starts now that its page is shown
    }
}
