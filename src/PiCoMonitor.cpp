#include <stdio.h>

#include "Com.h"
#include "OverlayWidget.h"
#include "Pages.h"
#include "Settings.h"
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
#include "pico_toolset/flash_store.h"

#ifdef RASPBERRYPI_PICO_W
	#include "pico/cyw43_arch.h"
#endif

#include <algorithm>
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
//! How long the page name stays on screen after a page switch (100 frames/s)
static const int PageToastFrames = 150;

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

	// static: the board owns large buffers (display line buffer, framebuffer
	// pointers...) that must not live on main()'s small stack
	static Board board;
	Screen screen(board.driver());

	// All pages and their widgets (see Pages.h); the Overview is shown first
	Pages pages(screen);

	// Full-screen slot is drawn last: used for the overlay ("NO SIGNAL" banner,
	// page-name toast)
	std::unique_ptr<OverlayWidget> overlay(new OverlayWidget(screen.driver().width(), screen.driver().height()));
	screen.set_widget(Screen::FS, overlay.get());

	// Backlight dimming state (was previously tracked inside the old Screen
	// base class; pico_toolset::Screen only exposes set_backlight(), so this
	// small state machine lives here instead).
	// Settings that survive a reboot (backlight level, current page), kept in the
	// last flash sectors (reserved in CMakeLists.txt) and written only after they
	// stop changing -- see Settings.h
	static pico_toolset::FlashStore flashStore;
	flashStore.init(pico_toolset::FlashStoreConfig::at_end_of_flash(PICOMONITOR_FLASH_SIZE, PICOMONITOR_SETTINGS_SECTORS));
	static SettingsStore settingsStore(flashStore);
	Settings settings;                       // defaults if nothing valid is stored
	settingsStore.load(settings);
	static SettingsSaver saver(settings, [](void* ctx, const Settings& s) { return static_cast<SettingsStore*>(ctx)->save(s); },
	                           &settingsStore);

	// Never boot into a (nearly) black screen, whatever was stored
	constexpr uint8_t MinBootBacklight = 20;
	uint8_t target_backlight = std::max(settings.backlight, MinBootBacklight);
	uint8_t backlight = target_backlight;
	board.set_backlight(backlight);
	pages.set_preferred(static_cast<Pages::Id>(settings.page));

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
		// dimmer (with auto-repeat); top-right / bottom-right = previous / next
		// page (press only, no repeat)
		const InputEvents input = board.poll_input();
		if (input.fired & CornerTopLeft)
			target_backlight = (target_backlight <= 255 - 10) ? target_backlight + 10 : 255;
		if (input.fired & CornerBottomLeft)
			target_backlight = (target_backlight >= 10) ? target_backlight - 10 : 0;
		if (input.pressed & (CornerTopRight | CornerBottomRight))
		{
			if (input.pressed & CornerTopRight)
				pages.prev();
			else
				pages.next();
			overlay->show_toast(pages.toast(), PageToastFrames);
			dirty = true;
		}

		// Poll the serial input; a complete valid frame updates the widgets
		MonitorData data;
		if (poll_frame(data))
		{
			dataStatus = computeStatus(data);

			// Update widget data (all pages)
			pages.update(data);

			lastUpdate = get_absolute_time();
			hadData = true;
			dirty = true;
		}

		absolute_time_t  now = get_absolute_time();
		auto diff = absolute_time_diff_us(lastUpdate, now);

		// Signal lost (only after having received data: at boot the widgets'
		// own placeholder labels are shown instead)
		const bool signalLost = hadData && diff >= NoSignalTime;
		if (signalLost != overlay->no_signal())
		{
			overlay->set_no_signal(signalLost);
			dirty = true;
		}
		board.set_status(signalLost ? Status::NoSignal : dataStatus);
		board.tick();

		// Animations of the visible page (falling CPU max markers, rotating disk
		// pages) and the page-name toast timing out keep the screen redrawing
		if (pages.tick())
			dirty = true;
		if (overlay->tick())
			dirty = true;

		// Render only on change
		if (dirty)
		{
			screen.update();
			board.present();
			dirty = false;
		}

		// Persist changed settings once they have settled
		Settings wanted;
		wanted.backlight = target_backlight;
		wanted.page = static_cast<uint8_t>(pages.preferred());
		saver.update(wanted, to_ms_since_boot(now));

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
