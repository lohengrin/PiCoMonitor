#include "ScreenPicoDisplay.h"

using namespace pimoroni;

//----------------------------------------------------------------------
ScreenPicoDisplay::ScreenPicoDisplay() :
    Screen(PicoDisplay::WIDTH, PicoDisplay::HEIGHT,
           std::make_unique<PicoGraphics_PenRGB565>(PicoDisplay::WIDTH, PicoDisplay::HEIGHT, nullptr)),
    st7789(PicoDisplay::WIDTH, PicoDisplay::HEIGHT, ROTATE_0, false, PiCoMonitor::ST7789EX::PIMORONI, get_spi_pins(BG_SPI_FRONT)),
    led(PicoDisplay::LED_R, PicoDisplay::LED_G, PicoDisplay::LED_B),
    button_a(PicoDisplay::A),
    button_b(PicoDisplay::B),
    ledColor(0,0,0)
{
    led.set_rgb(ledColor.r, ledColor.g, ledColor.b);
    set_target_backlight(DEFAULT_BACKLIGHT);
}

//----------------------------------------------------------------------
void ScreenPicoDisplay::update()
{
    st7789.update(graphics.get());
}

//----------------------------------------------------------------------
void ScreenPicoDisplay::apply_backlight(uint8_t val)
{
    st7789.set_backlight(val);
}

//----------------------------------------------------------------------
void ScreenPicoDisplay::onFrameBegin()
{
    // Backlight control with A/B buttons
    if (button_a.read())
        set_target_backlight((target_backlight() <= 255-10) ? target_backlight()+10 : 255);
    if (button_b.read())
        set_target_backlight((target_backlight() >= 10) ? target_backlight()-10 : 0);
}

//----------------------------------------------------------------------
void ScreenPicoDisplay::onDataReceived()
{
    cycleLed();
}

//----------------------------------------------------------------------
void ScreenPicoDisplay::cycleLed()
{
    if (ledColor.r == 0  && ledColor.g == 0  && ledColor.b == 0  ) ledColor = Color(LED_INTENSITY,0,0);
    else if (ledColor.r == LED_INTENSITY && ledColor.g == 0  && ledColor.b == 0  ) ledColor = Color(0,LED_INTENSITY,0);
    else if (ledColor.r == 0  && ledColor.g == LED_INTENSITY && ledColor.b == 0  ) ledColor = Color(0,0,LED_INTENSITY);
    else if (ledColor.r == 0  && ledColor.g == 0  && ledColor.b == LED_INTENSITY ) ledColor = Color(LED_INTENSITY,0,0);
    led.set_rgb(ledColor.r, ledColor.g, ledColor.b);
}
