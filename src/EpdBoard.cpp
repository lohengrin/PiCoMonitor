#include "EpdBoard.h"

#include "pico_toolset/epd_2in13_v4_configs.h"

#include "hardware/gpio.h"
#include "hardware/structs/ioqspi.h"
#include "hardware/structs/sio.h"
#include "hardware/sync.h"
#include "pico/stdlib.h"

#include <cstdio>

using namespace pico_toolset;

namespace {

// The BOOTSEL button shares the flash chip-select: reading it means briefly floating that pin
// (interrupts off), so this must not execute from flash. (Raspberry Pi's pico-examples recipe.)
bool __no_inline_not_in_flash_func(bootsel_pressed)() {
    constexpr uint cs_index = 1;
    const uint32_t flags = save_and_disable_interrupts();
    hw_write_masked(&ioqspi_hw->io[cs_index].ctrl, GPIO_OVERRIDE_LOW << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB,
                    IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS);
    for (int i = 0; i < 1000; ++i)
        __asm volatile("nop");
#ifdef PICO_RP2040
    constexpr uint32_t cs_bit = 1u << 1;
#else
    constexpr uint32_t cs_bit = SIO_GPIO_HI_IN_QSPI_CSN_BITS;
#endif
    const bool pressed = !(sio_hw->gpio_hi_in & cs_bit);
    hw_write_masked(&ioqspi_hw->io[cs_index].ctrl, GPIO_OVERRIDE_NORMAL << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB,
                    IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS);
    restore_interrupts(flags);
    return pressed;
}

} // namespace

uint8_t EpdBoard::s_framebuffer[Epd2in13V4::kFramebufferSize];

EpdBoard::EpdBoard() : m_driver(s_framebuffer, Epd2in13V4::kWidth, Epd2in13V4::kHeight) {
    if (!m_epd.init(configs::epd_2in13_v4::kWavesharePicoEpaper213))
        printf("EPD init FAILED\n");
}

InputEvents EpdBoard::poll_input() {
    InputEvents ev;
    const bool now = bootsel_pressed();
    if (now && !m_button_prev) {
        ev.pressed = ev.fired = CornerBottomRight;
        m_urgent = true;          // answer a button press without waiting
    }
    m_button_prev = now;
    return ev;
}

void EpdBoard::tick() {
    if (!m_pending)
        return;
    const uint32_t now = to_ms_since_boot(get_absolute_time());
    if (m_has_shown && !m_urgent && now - m_last_refresh_ms < kMinRefreshMs)
        return;
    if (m_has_shown && now - m_last_clear_ms >= kFullClearMs) {
        m_epd.clear_screen();     // the next update() is a full one
        m_last_clear_ms = now;
    }
    m_epd.update(s_framebuffer);  // blocks for the refresh; the panel sleeps afterwards
    m_pending = m_urgent = false;
    m_has_shown = true;
    m_last_refresh_ms = to_ms_since_boot(get_absolute_time());
    if (m_last_clear_ms == 0)
        m_last_clear_ms = m_last_refresh_ms;
}
