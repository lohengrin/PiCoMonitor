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
        TextWidget label(0, 0, "Disks", border, kColorBlack,
                          kGlyphFont5x8.glyphs, glyph_font_height, 2);
        label.set_transparent(true);
        label.set_centered(m_x + m_w / 2, m_y + m_h / 2);
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
        const uint8_t scale = spacing >= 20 ? 2 : 1;
        TextWidget text(0, 0, buf, bar.color(), kColorBlack,
                         kGlyphFont5x8.glyphs, glyph_font_height, scale);
        text.set_transparent(true);
        text.set_centered(m_x + label_w / 2, y);
        text.draw(display);
    }
}
