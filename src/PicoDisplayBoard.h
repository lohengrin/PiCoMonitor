#pragma once

#include "pico_toolset/st7789.h"
#include "pico_toolset/buffered_display.h"
#include "pico_toolset/reset_buttons.h"
#include "pico_toolset/rgb_led.h"

#include "Input.h"
#include "Status.h"

/// @brief Pimoroni Pico Display Pack: 240x135 display, 4 corner buttons, 1 RGB LED
/// (no touch, no SD card).
class PicoDisplayBoard {
public:
    static constexpr int WIDTH = 240;
    static constexpr int HEIGHT = 135;

    PicoDisplayBoard();

    pico_toolset::DisplayDriver& driver() { return m_driver; }

    void set_backlight(uint8_t val) { m_lcd.set_backlight(val); }

    //! Input events of this call (see Input.h). Call once per loop.
    InputEvents poll_input();
    //! Status shown on the RGB LED (NoSignal blinks)
    void set_status(Status s) { m_status = s; }
    //! Call once per loop: drives the LED blink
    void tick();
    //! BufferedDisplay::flush() already pushes pixels -- nothing extra here.
    void present() {}

private:
    void update_led(bool force);

    static uint16_t s_framebuffer[WIDTH * HEIGHT];

    pico_toolset::St7789 m_lcd;
    pico_toolset::BufferedDisplay m_driver;
    pico_toolset::RgbLed m_led;
    pico_toolset::DebouncedButtons m_buttons;
    RepeatFilter m_repeat;
    Status m_status = Status::Ok;
    int m_led_state = -1; // last LED color index written (-1: none yet)
};
