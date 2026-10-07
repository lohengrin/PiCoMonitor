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
    //! updated). A manual change: it also ends the cycling mode.
    void next() { step(+1); }
    void prev() { step(-1); }

    Id current() const { return m_current; }

    //! Manually selects `id` (a button, or the host): switches at once if that page is
    //! available, else as soon as it becomes available (an extra page only exists after the
    //! host has sent its data). Ends the cycling mode. A user switch (next/prev) cancels a
    //! pending request.
    void set_preferred(Id id);
    //! Boot: shows `id` as set_preferred() does but then (cycle_s > 0) resumes the cycling mode
    //! from it, once it is shown
    void restore(Id id, uint8_t cycle_s);
    //! The page to persist: the pending request if any, else the page last selected manually
    //! (in cycling mode: the page it started from, not the one showing now)
    Id preferred() const { return m_pending >= 0 ? static_cast<Id>(m_pending) : m_base; }
    bool available(Id id) const;
    //! "2/3 Network": position among the available pages and the page name
    std::string toast() const;

    //! Cycling mode: the pages are shown one after the other, every `seconds` (0 = off),
    //! starting from the current page. Any manual page change ends it.
    static constexpr uint8_t kDefaultCycleS = 30;
    void set_cycle(uint8_t seconds);
    //! Starts the cycling mode with the last period used (kDefaultCycleS at first)
    void start_cycle() { set_cycle(m_cycle_period); }
    //! Cycling period in seconds, 0 when the mode is off (this is what gets persisted)
    uint8_t cycle_s() const { return m_cycle_s; }
    //! "Cycling 30 s"
    std::string cycle_toast() const;
    //! Call once per main-loop iteration with a millisecond clock: moves to the next
    //! page when the cycling period has elapsed
    //! @return true when the page changed
    bool advance(uint32_t now_ms);

private:
    void step(int dir);
    int next_available(int from, int dir) const;
    void select(Id id);          // page chosen manually: shows it (or waits for it), becomes the persisted page
    void show(Id id);
    void apply();

    pico_toolset::Screen& m_screen;
    Id m_current = Overview;
    Id m_base = Overview;   // page last selected manually (persisted)
    int m_pending = -1;   // requested page not available yet (-1: none)
    uint8_t m_cycle_s = 0;                  // cycling period, 0 = off
    uint8_t m_cycle_period = kDefaultCycleS; // last period used
    bool m_cycle_arm = false;               // (re)start the cycle timer at the next advance()
    uint32_t m_cycle_deadline_ms = 0;
    bool m_seen_net = false, m_seen_system = false;
    size_t m_gpu_count = 0;   // most GPUs seen so far (0 = the GPU page does not exist yet)

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
    // GPU page with two GPUs: per GPU a load graph (top) and an info list (bottom)
    std::unique_ptr<GraphWidget> m_gpu2_load[2];
    std::unique_ptr<InfoListWidget> m_gpu2_info[2];
};
