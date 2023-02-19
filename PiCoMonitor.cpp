#include <stdio.h>

#include "Com.h"
#include "Screen.h"
#include "NullWidget.h"
#include "CPUWidget.h"
#include "GraphWidget.h"

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include <memory>

#define BUFFER_LENGTH 512

int main()
{
	stdio_init_all();

	Screen screen;

	screen.clear();

	std::unique_ptr<NullWidget> wbl(new NullWidget("BL"));
	std::unique_ptr<NullWidget> wbr(new NullWidget("BR"));
	std::unique_ptr<NullWidget> wul(new NullWidget("UL"));
	std::unique_ptr<NullWidget> wur(new NullWidget("UR"));

	std::unique_ptr<CPUWidget>  cpu(new CPUWidget());
	std::unique_ptr<GraphWidget>  temp(new GraphWidget(100,Color(255,0,0), "°C"));

	std::unique_ptr<NullWidget> full(new NullWidget("FF"));

//	screen.addWidget(wbl.get(),Screen::BL);
	screen.addWidget(wbr.get(),Screen::BR);
	screen.addWidget(wul.get(),Screen::UL);
//	screen.addWidget(wur.get(),Screen::UR);

	screen.addWidget(temp.get(),Screen::UR);
	screen.addWidget(cpu.get(),Screen::BL);

//	screen.addWidget(full.get(),Screen::FS);

	screen.draw();
	screen.update();

	// Init Wifi
	if (cyw43_arch_init())
	{
		printf("WiFi init failed");
		return -1;
	}

	int led = 0;
	char buffer[BUFFER_LENGTH];
	memset(buffer, 0, BUFFER_LENGTH);

	std::deque<double> temps;

	while (true)
	{
		// Read next message
		uint16_t len = get_data(buffer, BUFFER_LENGTH);
		if (len > 0)
		{
			// Blink led after each receive message
			// cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led);
			// led = (led) ? 0 : 1;

			// Decode data
			MonitorData data;
			if (!decode_data(buffer, len, data))
				continue;

			cpu->setValues(data.cpu_percent);
			temp->pushValue(data.temp);

#if 0
			// Display CPU perfo
			for ( int i = 0; i < data.cpu_percent.size(); ++i)
				screen.drawBar(screen.width() - (5+11*i), 0 , screen.height()/2, data.cpu_percent[i]/100.);

			const int graphSize = data.cpu_percent.size()*11 + 10;

			// Separator
			screen.drawLine(screen.width(), screen.height()/2 + 1, screen.width() - graphSize, screen.height()/2 + 1);

			// Temp graph
			temps.push_back(data.temp);

			while (temps.size() > graphSize)
				temps.pop_front();

			screen.drawGraph(screen.width()-graphSize, screen.height()/2 + 5, screen.width(), screen.height(), temps);
#endif

			screen.clear();
			screen.draw();
			screen.update();
		}
	}
	return 0;
}
