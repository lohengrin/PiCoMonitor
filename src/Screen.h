#pragma once

#include "pico_display.hpp"
#include "drivers/st7789/st7789.hpp"
#include "libraries/pico_graphics/pico_graphics.hpp"

#include "Widget.h"

#include <deque>

class Screen
{
    public:
        Screen();

        // Screen
        int width() const  { return pimoroni::PicoDisplay::WIDTH;   }
        int height() const { return pimoroni::PicoDisplay::HEIGHT;  }
        int xmax() const   { return pimoroni::PicoDisplay::WIDTH-1; }
        int ymax() const   { return pimoroni::PicoDisplay::HEIGHT-1;}
        int halfw() const   { return pimoroni::PicoDisplay::WIDTH/2; }
        int halfh() const   { return pimoroni::PicoDisplay::HEIGHT/2;}

        // Widget slots
        enum Slot {
            UL, // Uper Left
            BR, // Bottom Right
            UR, // Upper Right
            BL, // Bottom Left
            FS  // Full Screen
        };

        // Widget
        void addWidget(Widget * w, Slot pos);

        // Drawing
        void clear();
        void draw();
        void update();

        void drawBar(int x, int y, int size, float value);
        void drawLine(int x1, int y1, int x2, int y2);
        void drawGraph(int x1, int y1, int x2, int y2, std::deque<double>& data);

    private:
        pimoroni::ST7789 st7789;
        pimoroni::PicoGraphics_PenRGB565 graphics;

        pimoroni::Pen BG; // Background pen
        pimoroni::Pen BAR_G; // bar pen
        pimoroni::Pen BAR_Y; // bar pen
        pimoroni::Pen BAR_R; // bar pen
        pimoroni::Pen LINE; // line pen
        pimoroni::Pen GRAPH; // graph pen

        std::vector<Widget*> myWidgets;

};
