#pragma once

#include "pico_toolset/st7789.h"
#include "pico_toolset/buffered_display.h"
#include "pico_toolset/xpt2046.h"
#include "pico_toolset/sdcard.h"

#include "Input.h"
#include "Status.h"

/// @brief Elecrow CrowPanel PICO HMI 2.8": ST7789 SPI display + XPT2046
/// touch + uSD card, all sharing SPI1 with separate CS lines. See
/// third_party/pico-toolset/boards/crowpanel_pico_hmi_28.md.
class CrowPanelBoard {
public:
    static constexpr int WIDTH = 320;
    static constexpr int HEIGHT = 240;

    CrowPanelBoard();

    pico_toolset::DisplayDriver& driver() { return m_driver; }

    void set_backlight(uint8_t val) { m_lcd.set_backlight(val); }

    //! Touch and SD card are brought up (mounted) in the constructor but not
    //! polled/read by the main loop yet -- exposed here for whoever adds
    //! that next (e.g. touch-to-wake, or reading a background image/config
    //! off the uSD card).
    pico_toolset::Xpt2046Touch& touch() { return m_touch; }
    pico_toolset::SdCard& sd() { return m_sd; }

    //! Corners fired this call (see Input.h). Touch corner zones are not wired
    //! up yet (they need a measured touch calibration for this panel).
    uint8_t poll_input() { return 0; }
    //! This board has no status LED.
    void set_status(Status) {}
    void tick() {}
    //! BufferedDisplay::flush() (called by pico_toolset::Screen::update())
    //! already pushes pixels to the panel -- nothing extra to do here.
    void present() {}

private:
    static uint16_t s_framebuffer[WIDTH * HEIGHT];

    pico_toolset::St7789 m_lcd;
    pico_toolset::BufferedDisplay m_driver;
    pico_toolset::Xpt2046Touch m_touch;
    pico_toolset::SdCard m_sd;
};
