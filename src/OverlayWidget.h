#pragma once

#include "pico_toolset/widget.h"

#include <string>

/// @brief Full-screen overlay drawn on top of the current page (use the
/// Screen's full-screen slot, which is drawn last): the "NO SIGNAL" banner
/// (persistent while set) and a short "toast" at the bottom (page name after a
/// page switch) that disappears by itself after a while.
class OverlayWidget : public pico_toolset::Widget {
public:
    OverlayWidget(int width, int height) : m_w(width), m_h(height) {}

    void set_no_signal(bool v) { m_no_signal = v; }
    bool no_signal() const { return m_no_signal; }

    //! Show `text` for `frames` main-loop iterations (100 per second)
    void show_toast(const std::string& text, int frames) { m_toast = text; m_toast_frames = frames; }

    //! Call once per main-loop iteration
    //! @return true when the toast just disappeared (the screen needs redrawing)
    bool tick() {
        if (m_toast_frames > 0 && --m_toast_frames == 0) {
            m_toast.clear();
            return true;
        }
        return false;
    }

    void draw(pico_toolset::DisplayDriver& display) const override;

private:
    //! Black box with an accent border centered on (cx, cy) containing `text`
    void banner(pico_toolset::DisplayDriver& display, const std::string& text, int scale, int cx, int cy) const;

    int m_w, m_h;
    bool m_no_signal = false;
    std::string m_toast;
    int m_toast_frames = 0;
};
