#include "Screen.h"


Screen::Screen() :
    st7789(pimoroni::PicoDisplay::WIDTH, pimoroni::PicoDisplay::HEIGHT, ROTATE_0, false, get_spi_pins(BG_SPI_FRONT)),
    graphics(st7789.width, st7789.height, nullptr)
{

    
}
