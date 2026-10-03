#include "OverlayWidget.h"

#include "pico_toolset/simple_font.h"

using namespace pico_toolset;

void OverlayWidget::banner(DisplayDriver& display, const std::string& text, int scale, int cx, int cy) const {
    const Color accent = Color::from_rgb888(230, 126, 34);
    TextWidget t(0, 0, text.c_str(), accent, kColorBlack, kGlyphFont5x8.glyphs, glyph_font_height, scale);
    t.set_transparent(true);

    const int pad = 4 * scale;
    const int bw = t.text_width() + 2 * pad;
    const int bh = t.text_height() + 2 * pad;
    const int x0 = cx - bw / 2;
    const int y0 = cy - bh / 2;
    const int x1 = x0 + bw - 1;
    const int y1 = y0 + bh - 1;

    display.fill_rect(x0, y0, x1, y1, kColorBlack);
    display.draw_line(x0, y0, x1, y0, accent);
    display.draw_line(x1, y0, x1, y1, accent);
    display.draw_line(x1, y1, x0, y1, accent);
    display.draw_line(x0, y1, x0, y0, accent);

    t.set_centered(cx, cy);
    t.draw(display);
}

void OverlayWidget::draw(DisplayDriver& display) const {
    if (m_no_signal)
        banner(display, "NO SIGNAL", 2, m_w / 2, m_h / 2);

    if (!m_toast.empty()) {
        const int scale = m_w >= 300 ? 2 : 1;
        banner(display, m_toast, scale, m_w / 2, m_h - 8 * scale - 6 * scale);
    }
}
