#pragma once

#include "pico_toolset/epd_2in13_v4.h"

#include "Input.h"
#include "MonoDisplay.h"
#include "Status.h"

/// @brief Waveshare Pico-ePaper-2.13 (V4) on a Pico W: 250x122 black/white e-paper, no backlight,
/// no touch. The only input is the Pico's BOOTSEL button (= "next page").
///
/// The panel is slow (a partial refresh takes a fraction of a second, a full one ~2 s) and is
/// not meant to be refreshed continuously: the widgets draw into a 1 bpp buffer as on the other
/// boards, but the panel itself is only updated by tick(), at most every kMinRefreshMs, with a
/// full clear every kFullClearMs against ghosting. See third_party/pico-toolset/components/epd_2in13_v4.
class EpdBoard {
public:
    static constexpr int WIDTH = pico_toolset::Epd2in13V4::kHeight;   // landscape
    static constexpr int HEIGHT = pico_toolset::Epd2in13V4::kWidth;

    //! Shortest delay between two panel refreshes (a page switch refreshes at once)
    static constexpr uint32_t kMinRefreshMs = 1000;
    //! Full white clear (with blink) this often
    static constexpr uint32_t kFullClearMs = 30 * 60 * 1000;

    EpdBoard();

    pico_toolset::DisplayDriver& driver() { return m_driver; }

    //! No backlight
    void set_backlight(uint8_t) {}

    //! BOOTSEL = next page (press only)
    InputEvents poll_input();
    //! No status LED
    void set_status(Status) {}
    //! Call once per loop: sends the pending picture to the panel when it is time
    void tick();
    //! The widgets have just been drawn: the panel picture is now out of date
    void present() { m_pending = true; }

private:
    static uint8_t s_framebuffer[pico_toolset::Epd2in13V4::kFramebufferSize];

    pico_toolset::Epd2in13V4 m_epd;
    MonoDisplay m_driver;
    bool m_pending = false;
    bool m_urgent = false;          // refresh now, whatever the minimum delay
    bool m_has_shown = false;
    bool m_button_prev = false;
    uint32_t m_last_refresh_ms = 0;
    uint32_t m_last_clear_ms = 0;
};
