#include "Screen.h"

using namespace pimoroni;

SPIPins ElecrowPins {
  spi1,
  9U, // CS
  10U, // SPI CLK
  11U, // MOSI 
  12U, // MISO
  8U,  // DC 
  18U // BACK LIGHT PWM
};

//----------------------------------------------------------------------
Screen::Screen(int width, int height, PiCoMonitor::ST7789EX::Type type) :
    st7789(width, height, ROTATE_0, false, type, (type==PiCoMonitor::ST7789EX::PIMORONI)?get_spi_pins(BG_SPI_FRONT):ElecrowPins),
    graphics(width, height, nullptr),
    myWidth(width),
    myHeight(height)
{
    BG = graphics.create_pen(0,0,0);
    BAR_G = graphics.create_pen( 32, 180, 96);
    BAR_Y = graphics.create_pen(230, 126, 34);
    BAR_R = graphics.create_pen(255,  20, 15);
    LINE = graphics.create_pen(255, 255, 255);
    GRAPH = graphics.create_pen(0,  50, 255);
}

//----------------------------------------------------------------------
void Screen::addWidget(Widget *w, Slot pos)
{
    switch (pos) {
        default:
        case UL: w->setPosition(Point(0      , 0         ), Point(halfw()-1, halfh())); break;
        case UR: w->setPosition(Point(halfw(), 0         ), Point(xmax()   , halfh())); break;
        case BL: w->setPosition(Point(0      , halfh()+1 ), Point(halfw()-1, ymax() )); break;
        case BR: w->setPosition(Point(halfw(), halfh()+1 ), Point(xmax()   , ymax() )); break;
        case FS: w->setPosition(Point(0      , 0         ), Point(xmax()   , ymax() )); break;
    };
    w->setGraphics(&graphics);
    w->init();
    myWidgets.push_back(w);
}

//----------------------------------------------------------------------
void Screen::clear()
{
    graphics.set_pen(BG);
    graphics.clear();
}

//----------------------------------------------------------------------
void Screen::draw()
{
    for (auto w : myWidgets)
        w->draw();
}

//----------------------------------------------------------------------
void Screen::update()
{
    // update screen
    st7789.update(&graphics);
}

void Screen::set_backlight(uint8_t val)
{
    st7789.set_backlight(val);
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

void Screen::drawLine(int x1, int y1, int x2, int y2)
{
    graphics.set_pen(LINE);
    Point p1(x1,y1);
    Point p2(x2,y2);
    graphics.line(p1,p2);
}

void Screen::drawGraph(int x1, int y1, int x2, int y2, std::deque<double>& data)
{
    // Axis
    drawLine(x1, y1, x2, y1);
    drawLine(x1, y1, x1, y2);
    graphics.set_pen(GRAPH);
    for (int i = 0; i < data.size() ; ++i)
    {
        if (i > 0)
        {
            Point p1(x1+i,y1+data[i-1]);
            Point p2(x1+i+1,y1+data[i]);
            graphics.line(p1,p2);
        }
    }
}
