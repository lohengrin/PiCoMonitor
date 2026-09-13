#include "CPUWidget.h"

#include "pico_toolset/simple_font.h"

using namespace pico_toolset;

void CPUWidget::setValues(const std::vector<double>& cpus) {
    if (m_bars.size() != cpus.size()) {
        m_bars.clear();
        m_bars.reserve(cpus.size());
        if (!cpus.empty()) {
            int spacing = m_w / static_cast<int>(cpus.size());
            for (size_t i = 0; i < cpus.size(); ++i) {
                int bx = m_x + 1 + static_cast<int>(i) * spacing;
                int bw = spacing - 1;
                m_bars.emplace_back(bx, m_y + 1, bw, m_h - 2);
            }
        }
    }
    for (size_t i = 0; i < cpus.size(); ++i)
        m_bars[i].set_value(static_cast<float>(cpus[i] / 100.0));
}

void CPUWidget::draw(DisplayDriver& display) const {
    Color border = Color::from_rgb888(0, 50, 100);
    display.draw_line(m_x, m_y, m_x + m_w - 1, m_y, border);
    display.draw_line(m_x + m_w - 1, m_y, m_x + m_w - 1, m_y + m_h - 1, border);
    display.draw_line(m_x + m_w - 1, m_y + m_h - 1, m_x, m_y + m_h - 1, border);
    display.draw_line(m_x, m_y + m_h - 1, m_x, m_y, border);

    if (m_bars.empty()) {
        TextWidget label(m_x + m_w / 2 - 12, m_y + m_h / 2, "CPU", kColorWhite, kColorBlack,
                          kGlyphFont5x8.glyphs, glyph_font_height);
        label.draw(display);
        return;
    }

    for (const auto& bar : m_bars)
        bar.draw(display);
}
