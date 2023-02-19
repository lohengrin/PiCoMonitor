#pragma once

#include "pico_display.hpp"
#include "libraries/pico_graphics/pico_graphics.hpp"

class Widget {
public:
    Widget();

    // Called by Screen
    void setPosition(const pimoroni::Point& upperLeft, const pimoroni::Point& bottomRight);
    void setGraphics(pimoroni::PicoGraphics_PenRGB565* g);

    virtual void init() = 0;
    virtual void draw() = 0;

    // Utility
    int width() const { return abs(br.x - bl.x); }
    int height() const { return abs(ul.y - br.y); }

protected:
    pimoroni::PicoGraphics_PenRGB565* graphics;
    pimoroni::Point ul; // Upper Left
    pimoroni::Point br; // Bottom right
    pimoroni::Point ur; // Upper Right
    pimoroni::Point bl; // Bottom left
};
