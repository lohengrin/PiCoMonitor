#pragma once

#include "pico_toolset/widget.h"

/// @brief "NO SIGNAL" banner centered on the screen, drawn on top of the other
/// widgets (use the Screen's full-screen slot, which is drawn last). Invisible
/// until set_visible(true).
class NoSignalWidget : public pico_toolset::Widget {
public:
    NoSignalWidget(int width, int height) : m_w(width), m_h(height) {}

    void set_visible(bool v) { m_visible = v; }
    bool visible() const { return m_visible; }

    void draw(pico_toolset::DisplayDriver& display) const override;

private:
    int m_w, m_h;
    bool m_visible = false;
};
