#include "NoSignalWidget.h"

#include "pico_toolset/simple_font.h"

using namespace pico_toolset;

void NoSignalWidget::draw(DisplayDriver& display) const {
    if (!m_visible)
        return;

    const Color accent = Color::from_rgb888(230, 126, 34);
    TextWidget text(0, 0, "NO SIGNAL", accent, kColorBlack, kGlyphFont5x8.glyphs, glyph_font_height, 2);
    text.set_transparent(true);

    const int pad = 8;
    const int bw = text.text_width() + 2 * pad;
    const int bh = text.text_height() + 2 * pad;
    const int x0 = (m_w - bw) / 2;
    const int y0 = (m_h - bh) / 2;
    const int x1 = x0 + bw - 1;
    const int y1 = y0 + bh - 1;

    display.fill_rect(x0, y0, x1, y1, kColorBlack);
    display.draw_line(x0, y0, x1, y0, accent);
    display.draw_line(x1, y0, x1, y1, accent);
    display.draw_line(x1, y1, x0, y1, accent);
    display.draw_line(x0, y1, x0, y0, accent);

    text.set_centered(m_w / 2, m_h / 2);
    text.draw(display);
}
