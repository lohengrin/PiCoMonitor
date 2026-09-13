#pragma once

#include "pico_toolset/widget.h"

#include <vector>

/// @brief Per-core CPU load bars, composed from pico_toolset::BarWidget.
/// Bar geometry is (re)computed the first time setValues() sees a different
/// core count than before; each bar's own max-hold decay state then persists
/// across frames for that core slot.
class CPUWidget : public pico_toolset::Widget {
public:
    CPUWidget(int x, int y, int w, int h) : m_x(x), m_y(y), m_w(w), m_h(h) {}

    //! Values as % per core
    void setValues(const std::vector<double>& cpus);

    void draw(pico_toolset::DisplayDriver& display) const override;

private:
    int m_x, m_y, m_w, m_h;
    std::vector<pico_toolset::BarWidget> m_bars;
};
