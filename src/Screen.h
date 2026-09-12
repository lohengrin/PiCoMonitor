#pragma once

#include "libraries/pico_graphics/pico_graphics.hpp"

#include "Widget.h"

#include <deque>
#include <memory>
#include <vector>

/// @brief Default screen backlight level (0-255)
#define DEFAULT_BACKLIGHT 255

/// @brief Abstract screen. Each supported board derives from it and owns its
///        display driver (ST7789) and board-specific peripherals (LED, buttons).
class Screen
{
    public:
        virtual ~Screen() = default;

        // Screen geometry
        int width() const  { return myWidth;   }
        int height() const { return myHeight;  }
        int xmax() const   { return myWidth-1; }
        int ymax() const   { return myHeight-1;}
        int halfw() const   { return myWidth/2; }
        int halfh() const   { return myHeight/2;}

        //! Currently applied backlight level (0-255)
        uint8_t backlight() const { return myBacklight; }
        //! User backlight level; dimming temporarily lowers the applied level
        uint8_t target_backlight() const { return myTargetBacklight; }

        //! set_backlight: apply an immediate backlight level
        void set_backlight(uint8_t val);
        //! set_target_backlight: store the user backlight level and apply it
        void set_target_backlight(uint8_t val);

        // Widget slots
        enum Slot {
            UL, // Uper Left
            BR, // Bottom Right
            UR, // Upper Right
            BL, // Bottom Left
            FS  // Full Screen
        };

        //! Register a widget in one of the screen quadrants
        void addWidget(Widget * w, Slot pos);

        // Drawing
        //! Clear the graphics buffer with the background color
        void clear();
        //! Draw all registered widgets in the graphics buffer
        virtual void draw();
        //! Flush the graphics buffer to the LCD
        virtual void update() = 0;

        //! Called once per main loop, before frame rendering
        virtual void onFrameBegin() {}
        //! Called whenever a valid monitoring frame is received
        virtual void onDataReceived() {}

        // Drawing primitives
        void drawBar(int x, int y, int size, float value);
        void drawLine(int x1, int y1, int x2, int y2);
        void drawGraph(int x1, int y1, int x2, int y2, std::deque<double>& data);

    protected:
        Screen(int width, int height, std::unique_ptr<pimoroni::PicoGraphics> g);

        //! Apply a backlight value to the board hardware
        virtual void apply_backlight(uint8_t val) = 0;

        std::unique_ptr<pimoroni::PicoGraphics> graphics;

        pimoroni::Pen BG; // Background pen
        pimoroni::Pen BAR_G; // bar pen
        pimoroni::Pen BAR_Y; // bar pen
        pimoroni::Pen BAR_R; // bar pen
        pimoroni::Pen LINE; // line pen
        pimoroni::Pen GRAPH; // graph pen

        std::vector<Widget*> myWidgets;

        int myWidth;
        int myHeight;

    private:
        uint8_t myBacklight = DEFAULT_BACKLIGHT;
        uint8_t myTargetBacklight = DEFAULT_BACKLIGHT;
};
