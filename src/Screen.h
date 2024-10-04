#pragma once

#include "pico_display.hpp"
#include "st7789Ex/st7789Ex.hpp"
#include "libraries/pico_graphics/pico_graphics.hpp"

#include "Widget.h"

#include <deque>

class Screen
{
    public:
        Screen(int width, int height, PiCoMonitor::ST7789EX::Type type);

        // Screen
        int width() const  { return myWidth;   }
        int height() const { return myHeight;  }
        int xmax() const   { return myWidth-1; }
        int ymax() const   { return myHeight-1;}
        int halfw() const   { return myWidth/2; }
        int halfh() const   { return myHeight/2;}

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

        //! Back ligth control
        void set_backlight(uint8_t val);

        void drawBar(int x, int y, int size, float value);
        void drawLine(int x1, int y1, int x2, int y2);
        void drawGraph(int x1, int y1, int x2, int y2, std::deque<double>& data);

    private:
        PiCoMonitor::ST7789EX st7789;
        pimoroni::PicoGraphics_PenRGB565 graphics;
        //pimoroni::PicoGraphics_PenRGB332 graphics;

        pimoroni::Pen BG; // Background pen
        pimoroni::Pen BAR_G; // bar pen
        pimoroni::Pen BAR_Y; // bar pen
        pimoroni::Pen BAR_R; // bar pen
        pimoroni::Pen LINE; // line pen
        pimoroni::Pen GRAPH; // graph pen

        std::vector<Widget*> myWidgets;

        int myWidth;
        int myHeight;

};
