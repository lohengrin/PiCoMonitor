#include "PicoDisplayBoard.h"

#include "pico_toolset/st7789_configs.h"
#include "pico_toolset/reset_buttons_configs.h"
#include "pico_toolset/rgb_led_configs.h"

#include "pico/stdlib.h"

#include <cstdio>

using namespace pico_toolset;

namespace {
// Dim glow (same low intensity the LED has always had here)
constexpr uint8_t kLed = 25;
} // namespace

uint16_t PicoDisplayBoard::s_framebuffer[WIDTH * HEIGHT];

PicoDisplayBoard::PicoDisplayBoard() : m_driver(m_lcd, s_framebuffer) {
    if (!m_lcd.init(configs::st7789::kPimoroniPicoDisplayPack))
        printf("ST7789 init FAILED\n");
    m_led.init(configs::rgb_led::kPimoroniPicoDisplayPack);
    m_buttons.init(configs::buttons::kPimoroniPicoDisplayPack);
    update_led(true);
}

uint8_t PicoDisplayBoard::poll_input() {
    m_buttons.poll();
    // Button indices follow the corner bit order: A=TL, B=BL, X=TR, Y=BR
    return m_repeat.update(m_buttons.held_mask(), to_ms_since_boot(get_absolute_time()));
}

void PicoDisplayBoard::tick() {
    update_led(false);
}

void PicoDisplayBoard::update_led(bool force) {
    // 0 off, 1 green, 2 orange, 3 red
    int color = 0;
    switch (m_status) {
        case Status::Ok:       color = 1; break;
        case Status::Warning:  color = 2; break;
        case Status::Critical: color = 3; break;
        case Status::NoSignal: color = ((to_ms_since_boot(get_absolute_time()) / 500) % 2) ? 2 : 0; break;
    }
    if (!force && color == m_led_state)
        return;
    m_led_state = color;
    switch (color) {
        case 1:  m_led.set_rgb(0, kLed, 0); break;
        case 2:  m_led.set_rgb(kLed, kLed / 3, 0); break;
        case 3:  m_led.set_rgb(kLed, 0, 0); break;
        default: m_led.set_rgb(0, 0, 0); break;
    }
}
