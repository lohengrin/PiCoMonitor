#include "CrowPanelBoard.h"

#include "pico_toolset/st7789_configs.h"
#include "pico_toolset/xpt2046_configs.h"
#include "pico_toolset/sdcard_configs.h"
#include "pico_toolset/xpt2046_calibration.h"

#include "pico/stdlib.h"

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

InputEvents CrowPanelBoard::poll_input() {
    // A corner counts once the same one has been seen on kDebouncePolls
    // consecutive polls (a resistive panel gives spurious samples at touch-down)
    constexpr int kDebouncePolls = 3;

    uint8_t zone = 0;
    int x, y;
    if (xpt2046_to_pixel(m_touch.read(), configs::xpt2046::kElecrowCrowPanelPicoHmi28Calibration, WIDTH, HEIGHT, x, y))
        zone = corner_at(x, y, WIDTH, HEIGHT);

    m_zone_frames = (zone != 0 && zone == m_zone) ? m_zone_frames + 1 : 1;
    m_zone = zone;
    const uint8_t held = (zone != 0 && m_zone_frames >= kDebouncePolls) ? zone : 0;
    InputEvents ev;
    ev.fired = m_repeat.update(held, to_ms_since_boot(get_absolute_time()), &ev.pressed, &ev.longpressed);
    return ev;
}
