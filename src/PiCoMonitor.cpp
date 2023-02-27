#include <stdio.h>

#include "Com.h"
#include "Screen.h"
#include "NullWidget.h"
#include "CPUWidget.h"
#include "GraphWidget.h"
#include "DiskWidget.h"

// Pimodori display RGB led
#include "rgbled.hpp"
#include "button.hpp"

// PICO SDK
#include "pico/stdlib.h"
#ifdef RASPBERRYPI_PICO_W
#include "pico/cyw43_arch.h"
#endif

#include <memory>
#include <string.h>

#define BUFFER_LENGTH 512
#define LI 25 // Led intensity
#define DEFAULT_BACKLIGHT 128 // Screen intensity

using namespace pimoroni;

#define PERIOD_US 10000  // 100 Hz

int main()
{
	stdio_init_all();

#if 0
#ifdef RASPBERRYPI_PICO_W
	// Init Wifi if using PICO_W (not used yet)
	if (cyw43_arch_init())
	{
		printf("WiFi init failed");
		return -1;
	}
#endif
#endif

	// Screen initialization
	Screen screen;

	uint8_t backlight = DEFAULT_BACKLIGHT;
	screen.set_backlight(backlight);
	screen.clear();

	// Create Widgets
	std::unique_ptr<CPUWidget>  	cpu(new CPUWidget());
	std::unique_ptr<GraphWidget>  	temp(new GraphWidget(100,Color(10,10,255), "°C"));
	std::unique_ptr<GraphWidget>  	ram(new GraphWidget(100,Color(10,255,10), "RAM"));
	std::unique_ptr<DiskWidget>  	disks(new DiskWidget());

	screen.addWidget(ram.get(),Screen::UL);
	screen.addWidget(temp.get(),Screen::UR);
	screen.addWidget(cpu.get(),Screen::BL);
	screen.addWidget(disks.get(),Screen::BR);

	// First draw
	screen.draw();
	screen.update();

	// Communication buffer
	char buffer[BUFFER_LENGTH];
	memset(buffer, 0, BUFFER_LENGTH);

	// RGB Led control
	Color ledcolor(0,0,0);
	RGBLED led(PicoDisplay::LED_R, PicoDisplay::LED_G, PicoDisplay::LED_B);
	led.set_rgb(ledcolor.r, ledcolor.g, ledcolor.b);

	// Buttons
	Button button_a(PicoDisplay::A);
	Button button_b(PicoDisplay::B);
	Button button_x(PicoDisplay::X);
	Button button_y(PicoDisplay::Y);

	absolute_time_t  nextStep = delayed_by_us(get_absolute_time(),PERIOD_US);

	while (true)
	{
		// Read next message
		uint16_t len = get_data(buffer, BUFFER_LENGTH);
		if (len > 0) // Message is received
		{
			// Decode JSON data
			MonitorData data;
			if (!decode_data(buffer, len, data))
				continue;

			// LED Color cycling (change color for each valid receive frame)
			if (ledcolor.r == 0  && ledcolor.g == 0  && ledcolor.b == 0  ) ledcolor = Color(LI,0,0);
			else if (ledcolor.r == LI && ledcolor.g == 0  && ledcolor.b == 0  ) ledcolor = Color(0,LI,0);
			else if (ledcolor.r == 0  && ledcolor.g == LI && ledcolor.b == 0  ) ledcolor = Color(0,0,LI);
			else if (ledcolor.r == 0  && ledcolor.g == 0  && ledcolor.b == LI ) ledcolor = Color(LI,0,0);
			led.set_rgb(ledcolor.r, ledcolor.g, ledcolor.b);

			// Update widget data
			cpu->setValues(data.cpu_percent);
			temp->pushValue(data.temp);
			disks->setValues(data.disks);
			ram->pushValue(data.ram);
		}

		// Render
		screen.clear();
		screen.draw();
		screen.update();

		// Backlight control with A/B buttons
	    if(button_a.read())
		{
			backlight = (backlight <= 255-10)? backlight+10: 255;
			screen.set_backlight(backlight);
		}
	    if(button_b.read())
		{
			backlight = (backlight >= 10)? backlight-10: 0;
			screen.set_backlight(backlight);
		}

		// Wait next step according to PERIOD_US
		busy_wait_until(nextStep);
		nextStep = delayed_by_us(nextStep,PERIOD_US);
	}
	return 0;
}
