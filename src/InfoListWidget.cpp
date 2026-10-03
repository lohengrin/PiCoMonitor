#include "InfoListWidget.h"

#include "pico_toolset/simple_font.h"

#include <algorithm>

using namespace pico_toolset;

void InfoListWidget::draw(DisplayDriver& display) const {
    const Color border = Color::from_rgb888(0, 50, 100);
    display.draw_line(m_x, m_y, m_x + m_w - 1, m_y, border);
    display.draw_line(m_x + m_w - 1, m_y, m_x + m_w - 1, m_y + m_h - 1, border);
    display.draw_line(m_x + m_w - 1, m_y + m_h - 1, m_x, m_y + m_h - 1, border);
    display.draw_line(m_x, m_y + m_h - 1, m_x, m_y, border);

    if (m_rows.empty()) {
        TextWidget label(0, 0, m_title.c_str(), border, kColorBlack, kGlyphFont5x8.glyphs, glyph_font_height, 2);
        label.set_transparent(true);
        label.set_centered(m_x + m_w / 2, m_y + m_h / 2);
        label.draw(display);
        return;
    }

    const int rows = static_cast<int>(m_rows.size());
    const int pitch = (m_h - 4) / rows;
    const int pad = 4;

    // 2x if the rows are tall enough and the widest row fits, else 1x
    int scale = pitch >= 18 ? 2 : 1;
    for (const Row& r : m_rows) {
        const int chars = static_cast<int>(r.first.size() + r.second.size()) + 1;
        if (chars * 6 * scale - scale > m_w - 2 * pad) { scale = 1; break; }
    }

    const Color label_color = Color::from_rgb888(90, 140, 200);
    for (int i = 0; i < rows; ++i) {
        const int cy = m_y + 2 + i * pitch + pitch / 2;
        const int ty = cy - (8 * scale) / 2;

        TextWidget label(m_x + pad, ty, m_rows[i].first.c_str(), label_color, kColorBlack,
                          kGlyphFont5x8.glyphs, glyph_font_height, scale);
        label.set_transparent(true);
        label.draw(display);

        TextWidget value(0, ty, m_rows[i].second.c_str(), kColorWhite, kColorBlack,
                          kGlyphFont5x8.glyphs, glyph_font_height, scale);
        value.set_transparent(true);
        value.set_position(m_x + m_w - pad - value.text_width(), ty);
        value.draw(display);
    }
}
