#include <stdio.h>

#include "Com.h"
#include "Screen.h"
#include "NullWidget.h"
#include "CPUWidget.h"
#include "GraphWidget.h"
#include "DiskWidget.h"

// Platform header selected by CMake
#ifdef WITH_CROWPANEL
#include "ScreenCrowPanel.h"
#else
#include "ScreenPicoDisplay.h"
#endif

// PICO SDK
#include "pico/stdlib.h"

#ifdef RASPBERRYPI_PICO_W
	#include "pico/cyw43_arch.h"
#endif

#include <memory>
#include <string.h>

#define BUFFER_LENGTH 512

using namespace pimoroni;

#define PERIOD_US 10000  // 100 Hz

static const int64_t DimmingTime = 5000000; // 5 seconds
static const int64_t DimmingSpeed = 10000;  // 0.01 seconds

// Platform factory – single point of hardware selection
static std::unique_ptr<Screen> createScreen()
{
#ifdef WITH_CROWPANEL
	return std::make_unique<ScreenCrowPanel>();
#else
	return std::make_unique<ScreenPicoDisplay>();
#endif
}

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

	auto screen = createScreen();

	// Create Widgets
	std::unique_ptr<CPUWidget>  	cpu(new CPUWidget());
	std::unique_ptr<GraphWidget>  	temp(new GraphWidget(100,Color(10,10,255), "°C"));
	std::unique_ptr<GraphWidget>  	ram(new GraphWidget(100,Color(10,255,10), "RAM"));
	std::unique_ptr<DiskWidget>  	disks(new DiskWidget());

	screen->addWidget(ram.get(),Screen::UL);
	screen->addWidget(temp.get(),Screen::UR);
	screen->addWidget(cpu.get(),Screen::BL);
	screen->addWidget(disks.get(),Screen::BR);

	// First draw
	screen->clear();
	screen->draw();
	screen->update();

	// Communication buffer
	char buffer[BUFFER_LENGTH];
	memset(buffer, 0, BUFFER_LENGTH);

	absolute_time_t  nextStep = delayed_by_us(get_absolute_time(),PERIOD_US);
	absolute_time_t  lastUpdate = get_absolute_time();
	while (true)
	{
		// Platform hooks (button polling, LED feedback...)
		screen->onFrameBegin();

		// Read next message
		uint16_t len = get_data(buffer, BUFFER_LENGTH);
		if (len > 0) // Message is received
		{
			// Decode JSON data
			MonitorData data;
			if (!decode_data(buffer, len, data))
				continue;

			// Platform hook (LED color cycling)
			screen->onDataReceived();

			// Update widget data
			cpu->setValues(data.cpu_percent);
			temp->pushValue(data.temp);
			disks->setValues(data.disks);
			ram->pushValue(data.ram);

			lastUpdate = get_absolute_time();
		}

		// Render
		screen->clear();
		screen->draw();
		screen->update();

		// Manage dimming if no data
		absolute_time_t  now = get_absolute_time();
		auto diff = absolute_time_diff_us(lastUpdate, now);
		if (diff >= DimmingTime)
		{
			uint64_t dimDelta = floor((diff-DimmingTime)/DimmingSpeed);
			uint8_t cur = screen->backlight();
			if (dimDelta <= cur)
				screen->set_backlight(cur - dimDelta);
		}
		else if (screen->backlight() != screen->target_backlight())
		{
			screen->set_backlight(screen->target_backlight());
		}

		// Wait next step according to PERIOD_US
		busy_wait_until(nextStep);
		nextStep = delayed_by_us(nextStep,PERIOD_US);
	}
	return 0;
}
