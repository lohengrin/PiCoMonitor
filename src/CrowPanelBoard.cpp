#include "CrowPanelBoard.h"

#include "pico_toolset/st7789_configs.h"
#include "pico_toolset/xpt2046_configs.h"
#include "pico_toolset/sdcard_configs.h"

#include <cstdio>

using namespace pico_toolset;

uint16_t CrowPanelBoard::s_framebuffer[WIDTH * HEIGHT];

CrowPanelBoard::CrowPanelBoard() : m_driver(m_lcd, s_framebuffer) {
    bool lcd_ok = m_lcd.init(configs::st7789::kElecrowCrowPanelPicoHmi28);
    if (!lcd_ok) printf("ST7789 init FAILED\n");

    auto touch_cfg = configs::xpt2046::kElecrowCrowPanelPicoHmi28;
    touch_cfg.spi_instance = m_lcd.spi();
    m_touch.init(touch_cfg);

    if (!m_sd.init(configs::sdcard::kElecrowCrowPanelPicoHmi28))
        printf("SD mount failed (FRESULT=%d)\n", m_sd.last_mount_result());
}
