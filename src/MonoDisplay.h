#pragma once

// 1 bit-per-pixel landscape DisplayDriver for e-paper panels (pure C++, no Pico SDK: host-tested).
//
// The widgets are written for a dark screen (black background, bright colors); on paper it is the
// opposite, so a pixel becomes "ink" when its color is bright enough and everything black stays
// paper. Black text drawn over a colored bar (DiskWidget) thus comes out as paper-on-ink.
//
// The buffer is in the panel's native portrait layout (rows of NativeWidth bits, MSB leftmost,
// 1 = white), the landscape view is that panel rotated by 90 degrees (same mapping as Waveshare's
// GUI_Paint ROTATE_90): native x = NativeWidth - 1 - y, native y = x.

#include "pico_toolset/display_driver.h"

#include <cstdint>
#include <cstring>

class MonoDisplay : public pico_toolset::DisplayDriver {
public:
    //! @param buffer  caller-owned, ((native_w + 7) / 8) * native_h bytes
    MonoDisplay(uint8_t* buffer, int native_w, int native_h)
        : m_fb(buffer), m_nw(native_w), m_nh(native_h), m_row_bytes((native_w + 7) / 8) {
        std::memset(m_fb, 0xFF, static_cast<size_t>(m_row_bytes) * m_nh);
    }

    //! Landscape size
    int width() const override { return m_nh; }
    int height() const override { return m_nw; }

    //! Colors whose brightest channel (0..255) reaches this are drawn as ink
    static constexpr int kInkThreshold = 64;

    static bool is_ink(pico_toolset::Color c) {
        const int r = ((c.rgb565 >> 11) & 0x1F) << 3;
        const int g = ((c.rgb565 >> 5) & 0x3F) << 2;
        const int b = (c.rgb565 & 0x1F) << 3;
        return (r > g ? (r > b ? r : b) : (g > b ? g : b)) >= kInkThreshold;
    }

    void set_pixel(int x, int y, pico_toolset::Color color) override {
        if (x < 0 || y < 0 || x >= width() || y >= height()) return;
        const int nx = m_nw - 1 - y;
        const int ny = x;
        uint8_t& byte = m_fb[ny * m_row_bytes + nx / 8];
        const uint8_t mask = static_cast<uint8_t>(0x80 >> (nx & 7));
        if (is_ink(color))
            byte &= static_cast<uint8_t>(~mask);
        else
            byte |= mask;
    }

    const uint8_t* buffer() const { return m_fb; }

private:
    uint8_t* m_fb;
    int m_nw, m_nh, m_row_bytes;
};
