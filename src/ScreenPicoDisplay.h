#pragma once

#include "Screen.h"

#include "pico_display.hpp"
#include "st7789Ex/st7789Ex.hpp"
#include "rgbled.hpp"
#include "button.hpp"

/// @brief Pimoroni Pico Display Pack (240x135 RGB565, RGB LED + 4 buttons)
class ScreenPicoDisplay : public Screen
{
    public:
        ScreenPicoDisplay();
        virtual ~ScreenPicoDisplay() = default;

        virtual void update() override;
        virtual void onFrameBegin() override;
        virtual void onDataReceived() override;

    protected:
        virtual void apply_backlight(uint8_t val) override;

    private:
        PiCoMonitor::ST7789EX st7789;
        pimoroni::RGBLED led;
        pimoroni::Button button_a;
        pimoroni::Button button_b;

        static const uint8_t LED_INTENSITY = 25; // RGB LED intensity
        Color ledColor;                          // Current RGB LED color

        void cycleLed();
};
