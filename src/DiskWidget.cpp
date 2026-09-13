#include "DiskWidget.h"

#include "pico_toolset/simple_font.h"

#include <algorithm>

using namespace pico_toolset;

void DiskWidget::draw(DisplayDriver& display) const {
    Color border = Color::from_rgb888(0, 50, 100);
    display.draw_line(m_x, m_y, m_x + m_w - 1, m_y, border);
    display.draw_line(m_x + m_w - 1, m_y, m_x + m_w - 1, m_y + m_h - 1, border);
    display.draw_line(m_x + m_w - 1, m_y + m_h - 1, m_x, m_y + m_h - 1, border);
    display.draw_line(m_x, m_y + m_h - 1, m_x, m_y, border);

    if (m_values.empty()) {
        TextWidget label(m_x + m_w / 2 - 20, m_y + m_h / 2, "Disks", kColorWhite, kColorBlack,
                          kGlyphFont5x8.glyphs, glyph_font_height);
        label.draw(display);
        return;
    }

    const int count = static_cast<int>(m_values.size());
    const int spacing = m_h / count;
    const int label_w = 22;
    const int bar_w = std::max(1, m_w - label_w - 3);

    for (int i = 0; i < count; ++i) {
        const auto& d = m_values[static_cast<size_t>(count - 1 - i)];
        float ratio = d.total > 0 ? static_cast<float>(d.used / d.total) : 0.0f;
        int y = m_y + m_h - 1 - spacing / 2 - spacing * i;

        HBarWidget bar(m_x + label_w, y, bar_w, std::max(1, spacing / 2));
        bar.set_value(ratio);
        bar.draw(display);

        char buf[2] = {d.label.empty() ? '?' : d.label[0], 0};
        TextWidget text(m_x + 4, y - 4, buf, kColorWhite, kColorBlack,
                         kGlyphFont5x8.glyphs, glyph_font_height);
        text.draw(display);
    }
}
