#include "ScreenCrowPanel.h"

using namespace pimoroni;

// SPI interface shared by both boards (CrowPanel uses the alternate SPI instance)
SPIPins ElecrowPins {
  spi1,
  9U, // CS
  10U, // SPI CLK
  11U, // MOSI
  12U, // MISO
  8U,  // DC
  18U // BACK LIGHT PWM
};

//----------------------------------------------------------------------
ScreenCrowPanel::ScreenCrowPanel() :
    Screen(cWIDTH, cHEIGHT, std::make_unique<PicoGraphics_PenRGB332>(cWIDTH, cHEIGHT, nullptr))
{
    resetLcd();
    st7789 = std::make_unique<PiCoMonitor::ST7789EX>(cWIDTH, cHEIGHT, ROTATE_0, false, PiCoMonitor::ST7789EX::ELECROW, ElecrowPins);
    set_target_backlight(DEFAULT_BACKLIGHT);
}

//----------------------------------------------------------------------
void ScreenCrowPanel::update()
{
    st7789->update(graphics.get());
}

//----------------------------------------------------------------------
void ScreenCrowPanel::apply_backlight(uint8_t val)
{
    st7789->set_backlight(val);
}

//----------------------------------------------------------------------
void ScreenCrowPanel::resetLcd()
{
    // Reset LCD pin before initializing SPI LCD
    gpio_init(LCD_RESET_PIN);
    gpio_set_dir(LCD_RESET_PIN, GPIO_OUT);
    gpio_put(LCD_RESET_PIN, true);
    sleep_ms(5);
    gpio_put(LCD_RESET_PIN, false);
    sleep_ms(20);
    gpio_put(LCD_RESET_PIN, true);
}
