#pragma once

#include "pico_toolset/widget.h"
#include "Protocol.h"

#include <vector>

/// @brief Per-disk usage rows: a bar (pico_toolset::HBarWidget) with the full
/// disk label drawn inside it: black over the filled part, in the bar's own
/// color over the track (a glyph crossing the boundary changes color there).
/// When more disks than fit are present, they are shown in pages that rotate
/// automatically (see tick()).
class DiskWidget : public pico_toolset::Widget {
public:
    DiskWidget(int x, int y, int w, int h) : m_x(x), m_y(y), m_w(w), m_h(h) {}

    //! Values as % per disk
    void setValues(const std::vector<MonitorData::DiskData>& disks);

    //! Call once per main-loop iteration (100 Hz): rotates pages when the disks
    //! do not all fit.
    //! @return true when the displayed page changed (the widget needs redrawing)
    bool tick();

    void draw(pico_toolset::DisplayDriver& display) const override;

private:
    //! Smallest row height (pixels) that still fits an 8 px label in a bar
    static constexpr int kMinRow = 11;
    static constexpr int kFramesPerPage = 400; // 4 s at 100 Hz

    int rowsPerPage() const;
    int pageCount() const;

    int m_x, m_y, m_w, m_h;
    std::vector<MonitorData::DiskData> m_values;
    int m_page = 0;
    int m_frames = 0;
};
