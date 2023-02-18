#include <stdio.h>

#include "Com.h"
#include "Screen.h"

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#define BUFFER_LENGTH 512

int main()
{
	stdio_init_all();

	// Init Wifi
	if (cyw43_arch_init())
	{
		printf("WiFi init failed");
		return -1;
	}

	int led = 0;
	char buffer[BUFFER_LENGTH];
	memset(buffer, 0, BUFFER_LENGTH);

	Screen screen;

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

			// Display
			screen.clear();
			for ( int i = 0; i < data.cpu_percent.size(); ++i)
				screen.drawBar(screen.width() - (5+11*i), 0 , screen.height(), data.cpu_percent[i]/100.);
			screen.update();
		}
	}
	return 0;
}
