#pragma once

#include "pico_toolset/widget.h"
#include "Com.h"

#include <vector>

/// @brief Per-disk usage rows, composed from pico_toolset::HBarWidget plus a
/// single-character label per row.
class DiskWidget : public pico_toolset::Widget {
public:
    DiskWidget(int x, int y, int w, int h) : m_x(x), m_y(y), m_w(w), m_h(h) {}

    //! Values as % per disk
    void setValues(const std::vector<MonitorData::DiskData>& disks) { m_values = disks; }

    void draw(pico_toolset::DisplayDriver& display) const override;

private:
    int m_x, m_y, m_w, m_h;
    std::vector<MonitorData::DiskData> m_values;
};
