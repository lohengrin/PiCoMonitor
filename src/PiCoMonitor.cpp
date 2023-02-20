#include <stdio.h>

#include "Com.h"
#include "Screen.h"
#include "NullWidget.h"
#include "CPUWidget.h"
#include "GraphWidget.h"
#include "DiskWidget.h"

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "rgbled.hpp"

#include <memory>

#define BUFFER_LENGTH 512
#define LI 50 // Led intensity

using namespace pimoroni;

RGBLED led(PicoDisplay::LED_R, PicoDisplay::LED_G, PicoDisplay::LED_B);

int main()
{
	stdio_init_all();

	Screen screen;

	screen.clear();

	std::unique_ptr<NullWidget> wul(new NullWidget("UL"));

	std::unique_ptr<CPUWidget>  cpu(new CPUWidget());
	std::unique_ptr<GraphWidget>  temp(new GraphWidget(100,Color(10,10,255), "°C"));
	std::unique_ptr<GraphWidget>  ram(new GraphWidget(100,Color(10,255,10), "RAM"));
	std::unique_ptr<DiskWidget>  disks(new DiskWidget());

	screen.addWidget(ram.get(),Screen::UL);
	screen.addWidget(temp.get(),Screen::UR);
	screen.addWidget(cpu.get(),Screen::BL);
	screen.addWidget(disks.get(),Screen::BR);

	screen.draw();
	screen.update();

	// Init Wifi
	if (cyw43_arch_init())
	{
		printf("WiFi init failed");
		return -1;
	}

	char buffer[BUFFER_LENGTH];
	memset(buffer, 0, BUFFER_LENGTH);

	Color ledcolor(0,0,0);
	led.set_rgb(ledcolor.r, ledcolor.g, ledcolor.b);

	while (true)
	{
		// Read next message
		uint16_t len = get_data(buffer, BUFFER_LENGTH);
		if (len > 0)
		{
			// LED Color cycling
			if (ledcolor.r == 0  && ledcolor.g == 0  && ledcolor.b == 0  ) ledcolor = Color(LI,0,0);
			else if (ledcolor.r == LI && ledcolor.g == 0  && ledcolor.b == 0  ) ledcolor = Color(0,LI,0);
			else if (ledcolor.r == 0  && ledcolor.g == LI && ledcolor.b == 0  ) ledcolor = Color(0,0,LI);
			else if (ledcolor.r == 0  && ledcolor.g == 0  && ledcolor.b == LI ) ledcolor = Color(LI,0,0);
			led.set_rgb(ledcolor.r, ledcolor.g, ledcolor.b);

			// Decode data
			MonitorData data;
			if (!decode_data(buffer, len, data))
				continue;

			// Update data
			cpu->setValues(data.cpu_percent);
			temp->pushValue(data.temp);
			disks->setValues(data.disks);
			ram->pushValue(data.ram);

			// Render
			screen.clear();
			screen.draw();
			screen.update();
		}
	}
	return 0;
}
