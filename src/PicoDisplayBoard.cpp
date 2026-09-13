#include "PicoDisplayBoard.h"

#include "pico_toolset/st7789_configs.h"

#include <cstdio>

using namespace pico_toolset;

// Pico Display Pack pin numbers (A=12, B=13, LED_R=6, LED_G=7, LED_B=8) --
// the same values pico_display.hpp defines, inlined here to avoid pulling
// in that header (and its PicoGraphics-adjacent includes) for four constants.
namespace {
constexpr uint kButtonA = 12;
constexpr uint kButtonB = 13;
constexpr uint kLedR = 6;
constexpr uint kLedG = 7;
constexpr uint kLedB = 8;
} // namespace

uint16_t PicoDisplayBoard::s_framebuffer[WIDTH * HEIGHT];

PicoDisplayBoard::PicoDisplayBoard()
    : m_driver(m_lcd, s_framebuffer),
      m_led(kLedR, kLedG, kLedB),
      m_button_a(kButtonA),
      m_button_b(kButtonB) {
    if (!m_lcd.init(configs::st7789::kPimoroniPicoDisplayPack))
        printf("ST7789 init FAILED\n");
    m_led.set_rgb(0, 0, 0);
}

void PicoDisplayBoard::poll_buttons(uint8_t& target_backlight) {
    if (m_button_a.read())
        target_backlight = (target_backlight <= 255 - 10) ? target_backlight + 10 : 255;
    if (m_button_b.read())
        target_backlight = (target_backlight >= 10) ? target_backlight - 10 : 0;
}

void PicoDisplayBoard::on_data_received() {
    if (m_led_r == 0 && m_led_g == 0 && m_led_b == 0) { m_led_r = kLedIntensity; m_led_g = 0; m_led_b = 0; }
    else if (m_led_r == kLedIntensity && m_led_g == 0 && m_led_b == 0) { m_led_r = 0; m_led_g = kLedIntensity; m_led_b = 0; }
    else if (m_led_r == 0 && m_led_g == kLedIntensity && m_led_b == 0) { m_led_r = 0; m_led_g = 0; m_led_b = kLedIntensity; }
    else { m_led_r = kLedIntensity; m_led_g = 0; m_led_b = 0; }
    m_led.set_rgb(m_led_r, m_led_g, m_led_b);
}
