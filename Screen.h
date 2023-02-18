#pragma once

#include "pico_display.hpp"
#include "drivers/st7789/st7789.hpp"
#include "libraries/pico_graphics/pico_graphics.hpp"

class Screen
{
    public:
        Screen();

        int width() const;
        int height() const;

        // Drawing
        void clear();

        void drawBar(int x, int y, int size, float value);

        void update();

    private:
        pimoroni::ST7789 st7789;
        pimoroni::PicoGraphics_PenRGB332 graphics;

        pimoroni::Pen BG; // Background pen
        pimoroni::Pen BAR_G; // bar pen
        pimoroni::Pen BAR_Y; // bar pen
        pimoroni::Pen BAR_R; // bar pen

};
