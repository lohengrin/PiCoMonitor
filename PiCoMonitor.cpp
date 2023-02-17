#include <stdio.h>

#include "picojson.h"
#include "Com.h"

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
	uint8_t buffer[BUFFER_LENGTH];
	memset(buffer, 0, BUFFER_LENGTH);

	while (true)
	{
		// Read next message
		uint16_t len = get_data(buffer, BUFFER_LENGTH);
		if (len > 0)
		{
			cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led);
			led = (led) ? 0 : 1;
		}
	}
	return 0;
}
