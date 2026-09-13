#pragma once

#include "pico_toolset/widget.h"
#include "pico_toolset/simple_font.h"

#include <string>

/// @brief Thin wrapper around pico_toolset::LineGraphWidget with a border,
/// preserving the original scale/color/label constructor shape.
class GraphWidget : public pico_toolset::Widget {
public:
    GraphWidget(int x, int y, int w, int h, double scale, pico_toolset::Color color, const std::string& label)
        : m_x(x), m_y(y), m_w(w), m_h(h), m_label(label),
          m_graph(x + 2, y + 2, w - 4, h - 4, scale, color, m_label.c_str(),
                   pico_toolset::kGlyphFont5x8.glyphs, pico_toolset::glyph_font_height) {}

    //! Values as % per code
    void pushValue(double val) { m_graph.push_value(val); }

    void draw(pico_toolset::DisplayDriver& display) const override;

private:
    int m_x, m_y, m_w, m_h;
    std::string m_label;
    pico_toolset::LineGraphWidget m_graph;
};
