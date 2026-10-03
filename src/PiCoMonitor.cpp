#include <stdio.h>

#include "Com.h"
#include "CPUWidget.h"
#include "GraphWidget.h"
#include "DiskWidget.h"
#include "NoSignalWidget.h"
#include "Status.h"
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

using namespace pico_toolset;

#define PERIOD_US 10000  // 100 Hz

static const int64_t DimmingTime = 5000000; // 5 seconds
static const int64_t DimmingSpeed = 10000;  // 0.01 seconds
//! Without a valid frame for this long (after data was received at least once)
//! the "NO SIGNAL" banner is shown. Keep above the host's send period.
static const int64_t NoSignalTime = 3000000; // 3 seconds

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

	// Full-screen slot is drawn last: used for the "NO SIGNAL" overlay
	std::unique_ptr<NoSignalWidget> noSignal(new NoSignalWidget(screen.driver().width(), screen.driver().height()));
	screen.set_widget(Screen::FS, noSignal.get());

	// Backlight dimming state (was previously tracked inside the old Screen
	// base class; pico_toolset::Screen only exposes set_backlight(), so this
	// small state machine lives here instead).
	uint8_t backlight = 255;
	uint8_t target_backlight = 255;
	board.set_backlight(backlight);

	// The screen is only re-rendered when something changed (new data, a CPU
	// max marker still falling, the NO SIGNAL banner toggling).
	bool dirty = true;
	bool hadData = false;
	Status dataStatus = Status::Ok;

	absolute_time_t  nextStep = delayed_by_us(get_absolute_time(),PERIOD_US);
	absolute_time_t  lastUpdate = get_absolute_time();
	while (true)
	{
		// Corner inputs (buttons / touch zones): top-left brighter, bottom-left
		// dimmer; the right-hand corners are reserved for page switching
		const uint8_t corners = board.poll_input();
		if (corners & CornerTopLeft)
			target_backlight = (target_backlight <= 255 - 10) ? target_backlight + 10 : 255;
		if (corners & CornerBottomLeft)
			target_backlight = (target_backlight >= 10) ? target_backlight - 10 : 0;

		// Poll the serial input; a complete valid frame updates the widgets
		MonitorData data;
		if (poll_frame(data))
		{
			dataStatus = computeStatus(data);

			// Update widget data
			cpu->setValues(data.cpu_percent);
			if (data.has_temp)
				temp->pushValue(data.temp);
			disks->setValues(data.disks);
			if (data.has_ram)
				ram->pushValue(data.ram);

			lastUpdate = get_absolute_time();
			hadData = true;
			dirty = true;
		}

		absolute_time_t  now = get_absolute_time();
		auto diff = absolute_time_diff_us(lastUpdate, now);

		// Signal lost (only after having received data: at boot the widgets'
		// own placeholder labels are shown instead)
		const bool signalLost = hadData && diff >= NoSignalTime;
		if (signalLost != noSignal->visible())
		{
			noSignal->set_visible(signalLost);
			dirty = true;
		}
		board.set_status(signalLost ? Status::NoSignal : dataStatus);
		board.tick();

		// Falling CPU max markers keep the screen animating
		if (cpu->tick())
			dirty = true;

		// Disk pages rotate when not all disks fit
		if (disks->tick())
			dirty = true;

		// Render only on change
		if (dirty)
		{
			screen.update();
			board.present();
			dirty = false;
		}

		// Manage dimming if no data
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
