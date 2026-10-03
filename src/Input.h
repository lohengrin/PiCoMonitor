#pragma once

// Board-independent input model (pure C++, host-testable -- see tests/).
//
// Both supported boards expose the same four logical "corner" inputs:
//  * Pico Display Pack: its physical buttons A (top-left), B (bottom-left),
//    X (top-right), Y (bottom-right).
//  * CrowPanel: touch zones in the screen's corners.

#include <cstdint>

enum Corner : uint8_t {
    CornerTopLeft     = 1 << 0,
    CornerBottomLeft  = 1 << 1,
    CornerTopRight    = 1 << 2,
    CornerBottomRight = 1 << 3,
};

/// @brief Corner zone containing a pixel position (touch boards): each corner
/// is the outer third of both axes; the rest of the screen is no corner.
/// @return the Corner bit, or 0
inline uint8_t corner_at(int x, int y, int width, int height)
{
    const bool left   = x < width / 3;
    const bool right  = x >= width - width / 3;
    const bool top    = y < height / 3;
    const bool bottom = y >= height - height / 3;
    if (top && left)     return CornerTopLeft;
    if (bottom && left)  return CornerBottomLeft;
    if (top && right)    return CornerTopRight;
    if (bottom && right) return CornerBottomRight;
    return 0;
}

/// @brief Turns "which corners are held" into press events with auto-repeat:
/// a corner fires when first pressed, again after kRepeatMs while held, and
/// three times faster once held longer than kHoldMs (the behavior of the
/// Pimoroni button driver this replaces).
class RepeatFilter {
public:
    static constexpr uint32_t kRepeatMs = 200;
    static constexpr uint32_t kHoldMs = 1000;

    //! @param held   mask of corners currently held down
    //! @param now_ms monotonic milliseconds
    //! @return mask of corners that fire on this call
    uint8_t update(uint8_t held, uint32_t now_ms) {
        uint8_t fired = 0;
        for (int i = 0; i < 4; ++i) {
            const uint8_t bit = static_cast<uint8_t>(1u << i);
            if (!(held & bit)) {
                m_down &= static_cast<uint8_t>(~bit);
                continue;
            }
            if (!(m_down & bit)) {              // new press
                m_down |= bit;
                m_pressed_ms[i] = m_last_ms[i] = now_ms;
                fired |= bit;
                continue;
            }
            uint32_t rate = kRepeatMs;
            if (now_ms - m_pressed_ms[i] > kHoldMs)
                rate /= 3;
            if (now_ms - m_last_ms[i] > rate) { // auto-repeat
                m_last_ms[i] = now_ms;
                fired |= bit;
            }
        }
        return fired;
    }

private:
    uint8_t  m_down = 0;
    uint32_t m_pressed_ms[4] = {};
    uint32_t m_last_ms[4] = {};
};
