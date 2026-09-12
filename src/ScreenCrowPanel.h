#pragma once

#include "Screen.h"

#include "st7789Ex/st7789Ex.hpp"

#include <memory>

/// @brief Elecrow CrowPanel 2.8" HMI (320x240 RGB332, no LED/buttons, LDC reset on GPIO15)
class ScreenCrowPanel : public Screen
{
    public:
        ScreenCrowPanel();
        virtual ~ScreenCrowPanel() = default;

        virtual void update() override;

    protected:
        virtual void apply_backlight(uint8_t val) override;

    private:
        static constexpr int cWIDTH  = 320;
        static constexpr int cHEIGHT = 240;
        static constexpr uint LCD_RESET_PIN = 15; // CrowPanel LCD reset

        //! Toggle the LCD reset pin before initializing the SPI display
        void resetLcd();

        std::unique_ptr<PiCoMonitor::ST7789EX> st7789;
};
