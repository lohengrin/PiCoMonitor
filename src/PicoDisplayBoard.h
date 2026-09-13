#pragma once

#include "pico_toolset/st7789.h"
#include "pico_toolset/st7789_driver.h"

#include "rgbled.hpp"
#include "button.hpp"

/// @brief Pimoroni Pico Display Pack (240x135 RGB LED + 2 buttons used here).
class PicoDisplayBoard {
public:
    static constexpr int WIDTH = 240;
    static constexpr int HEIGHT = 135;

    PicoDisplayBoard();

    pico_toolset::DisplayDriver& driver() { return m_driver; }

    void set_backlight(uint8_t val) { m_lcd.set_backlight(val); }

    //! Backlight adjustment via the A/B buttons, called once per main loop.
    void poll_buttons(uint8_t& target_backlight);
    //! Cycles the RGB LED through R -> G -> B on each received data frame.
    void on_data_received();
    //! St7789Driver::flush() already pushes pixels -- nothing extra here.
    void present() {}

private:
    static uint16_t s_framebuffer[WIDTH * HEIGHT];
    static constexpr uint8_t kLedIntensity = 25;

    pico_toolset::St7789 m_lcd;
    pico_toolset::St7789Driver m_driver;
    pimoroni::RGBLED m_led;
    pimoroni::Button m_button_a;
    pimoroni::Button m_button_b;
    uint8_t m_led_r = 0, m_led_g = 0, m_led_b = 0;
};
