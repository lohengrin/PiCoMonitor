#include <stdio.h>

#include "Com.h"
#include "CPUWidget.h"
#include "GraphWidget.h"
#include "DiskWidget.h"
#include "pico_toolset/screen.h"

// Platform header selected by CMake
#ifdef WITH_CROWPANEL
#include "CrowPanelBoard.h"
using Board = CrowPanelBoard;
#else
#include "PicoDisplayBoard.h"
using Board = PicoDisplayBoard;
#endif

// PICO SDK
#include "pico/stdlib.h"

#ifdef RASPBERRYPI_PICO_W
	#include "pico/cyw43_arch.h"
#endif

#include <memory>
#include <string.h>
#include <cmath>

#define BUFFER_LENGTH 512

using namespace pico_toolset;

#define PERIOD_US 10000  // 100 Hz

static const int64_t DimmingTime = 5000000; // 5 seconds
static const int64_t DimmingSpeed = 10000;  // 0.01 seconds

int main()
{
	stdio_init_all();

#ifdef RASPBERRYPI_PICO_W
	// Init Wifi if using PICO_W (not used yet)
	if (cyw43_arch_init())
	{
		printf("WiFi init failed");
		return -1;
	}
#endif

	Board board;
	Screen screen(board.driver());

	// Quadrant rects, computed once from the driver's real geometry --
	// pico_toolset::Screen doesn't auto-position widgets the way the old
	// local Screen/Widget classes did, so this is done explicitly here.
	Screen::SlotRect ul = screen.slot_rect(Screen::UL);
	Screen::SlotRect ur = screen.slot_rect(Screen::UR);
	Screen::SlotRect bl = screen.slot_rect(Screen::BL);
	Screen::SlotRect br = screen.slot_rect(Screen::BR);

	// Create Widgets
	std::unique_ptr<CPUWidget>   cpu(new CPUWidget(bl.x, bl.y, bl.w, bl.h));
	std::unique_ptr<GraphWidget> temp(new GraphWidget(ur.x, ur.y, ur.w, ur.h, 100,
	                                                   Color::from_rgb888(10, 10, 255), "\xC2\xB0" "C"));
	std::unique_ptr<GraphWidget> ram(new GraphWidget(ul.x, ul.y, ul.w, ul.h, 100,
	                                                  Color::from_rgb888(10, 255, 10), "RAM"));
	std::unique_ptr<DiskWidget>  disks(new DiskWidget(br.x, br.y, br.w, br.h));

	screen.set_widget(Screen::UL, ram.get());
	screen.set_widget(Screen::UR, temp.get());
	screen.set_widget(Screen::BL, cpu.get());
	screen.set_widget(Screen::BR, disks.get());

	// Backlight dimming state (was previously tracked inside the old Screen
	// base class; pico_toolset::Screen only exposes set_backlight(), so this
	// small state machine lives here instead).
	uint8_t backlight = 255;
	uint8_t target_backlight = 255;
	board.set_backlight(backlight);

	// First draw
	screen.update();
	board.present();

	// Communication buffer
	char buffer[BUFFER_LENGTH];
	memset(buffer, 0, BUFFER_LENGTH);

	absolute_time_t  nextStep = delayed_by_us(get_absolute_time(),PERIOD_US);
	absolute_time_t  lastUpdate = get_absolute_time();
	while (true)
	{
		// Platform hooks (button polling, LED feedback...)
		board.poll_buttons(target_backlight);

		// Read next message
		uint16_t len = get_data(buffer, BUFFER_LENGTH);
		if (len > 0) // Message is received
		{
			// Decode JSON data
			MonitorData data;
			if (!decode_data(buffer, len, data))
				continue;

			// Platform hook (LED color cycling)
			board.on_data_received();

			// Update widget data
			cpu->setValues(data.cpu_percent);
			temp->pushValue(data.temp);
			disks->setValues(data.disks);
			ram->pushValue(data.ram);

			lastUpdate = get_absolute_time();
		}

		// Render
		screen.update();
		board.present();

		// Manage dimming if no data
		absolute_time_t  now = get_absolute_time();
		auto diff = absolute_time_diff_us(lastUpdate, now);
		if (diff >= DimmingTime)
		{
			uint64_t dimDelta = floor((diff-DimmingTime)/DimmingSpeed);
			if (dimDelta <= backlight)
			{
				backlight = backlight - dimDelta;
				board.set_backlight(backlight);
			}
		}
		else if (backlight != target_backlight)
		{
			backlight = target_backlight;
			board.set_backlight(backlight);
		}

		// Wait next step according to PERIOD_US
		busy_wait_until(nextStep);
		nextStep = delayed_by_us(nextStep,PERIOD_US);
	}
	return 0;
}
