#include "DiskWidget.h"

#include "pico_toolset/simple_font.h"

#include <algorithm>
#include <string>

using namespace pico_toolset;

int DiskWidget::rowsPerPage() const {
    return std::max(1, (m_h - 2) / kMinRow);
}

int DiskWidget::pageCount() const {
    const int n = static_cast<int>(m_values.size());
    const int rpp = rowsPerPage();
    return std::max(1, (n + rpp - 1) / rpp);
}

void DiskWidget::setValues(const std::vector<MonitorData::DiskData>& disks) {
    m_values = disks;
    if (m_page >= pageCount())
        m_page = 0;
}

bool DiskWidget::tick() {
    if (pageCount() <= 1) {
        m_frames = 0;
        return false;
    }
    if (++m_frames < kFramesPerPage)
        return false;
    m_frames = 0;
    m_page = (m_page + 1) % pageCount();
    return true;
}

// "LABEL [XX%]" fitted into max_px (fixed-width font: 5 px glyph + 1 px spacing).
// Only the label is shortened (with ".."), so the percentage is always shown.
static std::string fit_label(const std::string& label, int percent, int max_px, int scale) {
    const std::string suffix = " [" + std::to_string(percent) + "%]";
    const int char_px = 6 * scale;
    const int max_chars = std::max(1, (max_px + scale) / char_px);
    const int room = std::max(1, max_chars - static_cast<int>(suffix.size()));
    if (static_cast<int>(label.size()) <= room)
        return label + suffix;
    if (room <= 2)
        return label.substr(0, room) + suffix;
    return label.substr(0, room - 2) + ".." + suffix;
}

void DiskWidget::draw(DisplayDriver& display) const {
    Color border = Color::from_rgb888(0, 50, 100);
    display.draw_line(m_x, m_y, m_x + m_w - 1, m_y, border);
    display.draw_line(m_x + m_w - 1, m_y, m_x + m_w - 1, m_y + m_h - 1, border);
    display.draw_line(m_x + m_w - 1, m_y + m_h - 1, m_x, m_y + m_h - 1, border);
    display.draw_line(m_x, m_y + m_h - 1, m_x, m_y, border);

    if (m_values.empty()) {
        TextWidget label(0, 0, "Disks", border, kColorBlack,
                          kGlyphFont5x8.glyphs, glyph_font_height, 2);
        label.set_transparent(true);
        label.set_centered(m_x + m_w / 2, m_y + m_h / 2);
        label.draw(display);
        return;
    }

    const int n = static_cast<int>(m_values.size());
    const int rpp = rowsPerPage();
    const int first = m_page * rpp;
    const int rows = std::min(rpp, n - first);

    const int spacing = (m_h - 2) / rows;          // row pitch
    const int thickness = std::max(8, spacing - 2); // bar height (>= text height)
    const int scale = thickness >= 18 ? 2 : 1;
    const int bar_x = m_x + 2;
    const int bar_w = m_w - 4;

    for (int j = 0; j < rows; ++j) {
        const auto& d = m_values[static_cast<size_t>(first + j)];
        const float ratio = d.total > 0 ? static_cast<float>(d.used / d.total) : 0.0f;

        // Row centered in its slot; HBarWidget draws centered on y
        const int cy = m_y + 1 + j * spacing + spacing / 2;
        HBarWidget bar(bar_x, cy, bar_w, thickness);
        bar.set_value(ratio);
        bar.draw(display);

        const int percent = std::clamp(static_cast<int>(ratio * 100.0f + 0.5f), 0, 100);
        const std::string text = fit_label(d.label.empty() ? "?" : d.label, percent, bar_w - 6, scale);
        TextWidget label(bar_x + 3, cy - (8 * scale) / 2, text.c_str(), kColorBlack, kColorBlack,
                          kGlyphFont5x8.glyphs, glyph_font_height, scale);
        label.set_transparent(true);
        label.draw(display);
    }
}
