#include "Screen.h"

using namespace pimoroni;

Screen::Screen() :
    st7789(PicoDisplay::WIDTH, PicoDisplay::HEIGHT, ROTATE_0, false, get_spi_pins(BG_SPI_FRONT)),
    graphics(st7789.width, st7789.height, nullptr)
{

    
}
