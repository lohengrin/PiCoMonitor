#include "Widget.h"

using namespace pimoroni;

//--------------------------------------------------------------------------------------------
Widget::Widget() : 
graphics(nullptr),
ul(0,0),
br(100,100)
{
}

//--------------------------------------------------------------------------------------------
void Widget::setPosition(const pimoroni::Point& upperLeft, const pimoroni::Point& bottomRight)
{
    ul = upperLeft;
    br = bottomRight;
    bl = Point(ul.x, br.y);
    ur = Point(br.x, ul.y);
}

//--------------------------------------------------------------------------------------------
void Widget::setGraphics(pimoroni::PicoGraphics_PenRGB565* g)
{
    graphics = g;
}
