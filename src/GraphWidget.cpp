#include "GraphWidget.h"

using namespace pico_toolset;

void GraphWidget::draw(DisplayDriver& display) const {
    Color border = Color::from_rgb888(0, 50, 100);
    display.draw_line(m_x, m_y, m_x + m_w - 1, m_y, border);
    display.draw_line(m_x + m_w - 1, m_y, m_x + m_w - 1, m_y + m_h - 1, border);
    display.draw_line(m_x + m_w - 1, m_y + m_h - 1, m_x, m_y + m_h - 1, border);
    display.draw_line(m_x, m_y + m_h - 1, m_x, m_y, border);

    m_graph.draw(display);
}
