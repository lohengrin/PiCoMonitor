#pragma once

#include "pico_display.hpp"
#include "drivers/st7789/st7789.hpp"
#include "libraries/pico_graphics/pico_graphics.hpp"

#include <deque>

class Screen
{
    public:
        Screen();

        int width() const;
        int height() const;

        // Drawing
        void clear();

        void drawBar(int x, int y, int size, float value);
        void drawLine(int x1, int y1, int x2, int y2);

        void drawGraph(int x1, int y1, int x2, int y2, std::deque<double>& data);

        void update();

    private:
        pimoroni::ST7789 st7789;
        pimoroni::PicoGraphics_PenRGB332 graphics;

        pimoroni::Pen BG; // Background pen
        pimoroni::Pen BAR_G; // bar pen
        pimoroni::Pen BAR_Y; // bar pen
        pimoroni::Pen BAR_R; // bar pen
        pimoroni::Pen LINE; // line pen
        pimoroni::Pen GRAPH; // graph pen

};
