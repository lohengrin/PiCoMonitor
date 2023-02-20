#pragma once

#include "pico_display.hpp"
#include "libraries/pico_graphics/pico_graphics.hpp"

/// @brief Utility to store a color (RGB)
struct Color {
    Color(int _r=0, int _g=0, int _b=0) : r(_r), g(_g), b(_b) {}
    uint8_t r,g,b;
};

/// @brief Base abstrac class for Widgets.
class Widget {
public:
    //! Constructor
    Widget();

    //! Called by Screen when adding widget to screen
    //! setPosition: set the widget position
    void setPosition(const pimoroni::Point& upperLeft, const pimoroni::Point& bottomRight);
    //! setGraphics: give the graphics object to widget to draw
    void setGraphics(pimoroni::PicoGraphics_PenRGB565* g);

    //! Called when setPosition and setGraphics is done
    virtual void init() = 0;
    //! Called at each screen refresh
    virtual void draw() = 0;

    // Utility
    int width() const { return abs(br.x - bl.x); }
    int height() const { return abs(ul.y - br.y); }

protected:
    pimoroni::PicoGraphics_PenRGB565* graphics;
    pimoroni::Point ul; // Widget's Upper Left position on screen
    pimoroni::Point br; // Widget's Bottom right position on screen
    pimoroni::Point ur; // Widget's Upper Right position on screen
    pimoroni::Point bl; // Widget's Bottom left position on screen
};
