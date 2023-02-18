#include "Screen.h"

using namespace pimoroni;

Screen::Screen() :
    st7789(PicoDisplay::WIDTH, PicoDisplay::HEIGHT, ROTATE_0, false, get_spi_pins(BG_SPI_FRONT)),
    graphics(st7789.width, st7789.height, nullptr)
{
    BG = graphics.create_pen(0,0,50);
    BAR_G = graphics.create_pen( 32, 180, 96);
    BAR_Y = graphics.create_pen(230, 126, 34);
    BAR_R = graphics.create_pen(255,  57, 43);
}

int Screen::width() const
{
    return  PicoDisplay::WIDTH;   
}

int Screen::height() const
{
    return  PicoDisplay::HEIGHT;
}

void Screen::clear()
{
    graphics.set_pen(BG);
    graphics.clear();
}

void Screen::drawBar(int x, int y, int size, float value)
{
    if (value < 0.5) 
        graphics.set_pen(BAR_G);
    else if (value >= 0.5 && value < 0.8 ) 
        graphics.set_pen(BAR_Y);
    else 
        graphics.set_pen(BAR_R);

    int sizeval = (float)size * value;

    Point p1(x,y);
    Point p2(x,std::max(y+1,y + sizeval));

    graphics.thick_line(p1, p2, 10);
}

void Screen::update()
{
    // update screen
    st7789.update(&graphics);
}
