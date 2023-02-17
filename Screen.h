#pragma once

#include "pico_display.hpp"
#include "drivers/st7789/st7789.hpp"
#include "libraries/pico_graphics/pico_graphics.hpp"

class Screen
{
    public:
        Screen();


    private:
        pimoroni::ST7789 st7789;
        pimoroni::PicoGraphics_PenRGB332 graphics;

};
