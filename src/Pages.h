#pragma once

// Page manager (pure widget logic, no Pico SDK: host-testable, see tests/).
//
// Pages: Overview (always), Network (network + disk I/O graphs), System (list
// of CPU frequency / cores / load / swap / uptime), GPU. The extra pages
// become available the first time the host sends the matching data, so an old
// host or a platform without e.g. a GPU simply never shows them.

#include "pico_toolset/screen.h"
#include "CPUWidget.h"
#include "DiskWidget.h"
#include "GraphWidget.h"
#include "InfoListWidget.h"
#include "Protocol.h"

#include <memory>
#include <string>

class Pages {
public:
    enum Id { Overview, Network, System, Gpu, kCount };

    explicit Pages(pico_toolset::Screen& screen);

    //! Feed one decoded frame to every page's widgets
    void update(const MonitorData& data);

    //! Call once per main-loop iteration (animations of the visible page)
    //! @return true when the visible page needs redrawing
    bool tick();

    //! Switch to the next / previous available page (the screen's slots are
    //! updated)
    void next() { step(+1); }
    void prev() { step(-1); }

    Id current() const { return m_current; }
    bool available(Id id) const;
    //! "2/3 Network": position among the available pages and the page name
    std::string toast() const;

private:
    void step(int dir);
    void apply();

    pico_toolset::Screen& m_screen;
    Id m_current = Overview;
    bool m_seen_net = false, m_seen_system = false, m_seen_gpu = false;

    // Overview
    std::unique_ptr<CPUWidget> m_cpu;
    std::unique_ptr<GraphWidget> m_temp, m_ram;
    std::unique_ptr<DiskWidget> m_disks;
    // Network
    std::unique_ptr<GraphWidget> m_net_down, m_net_up, m_io_read, m_io_write;
    // System
    std::unique_ptr<InfoListWidget> m_system;
    // GPU
    std::unique_ptr<GraphWidget> m_gpu_load, m_gpu_temp, m_gpu_vram;
    std::unique_ptr<InfoListWidget> m_gpu_info;
};
