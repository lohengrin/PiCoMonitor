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
	char buffer[BUFFER_LENGTH];
	memset(buffer, 0, BUFFER_LENGTH);

	while (true)
	{
		// Read next message
		uint16_t len = get_data(buffer, BUFFER_LENGTH);
		if (len > 0)
		{
			// Blink led after each receive message
			cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led);
			led = (led) ? 0 : 1;

			// Decode data
			std::vector<double> cpu_percent;
			double temp = 0.0;
			double ram = 0.0;

			picojson::value v;
			std::string err = picojson::parse(v, std::string(buffer));
			if (! err.empty()) continue;
			if (! v.is<picojson::object>()) continue;

			const auto& obj = v.get<picojson::object>();
			for (auto&& i = obj.begin(); i != obj.end(); ++i) 
			{
				if (i->first == "CPU" && i->second.is<picojson::array>())
				{
					const auto& arr = i->second.get<picojson::array>();
					for (auto&& j = arr.begin(); j != arr.end(); ++j) 
						cpu_percent.push_back(j->get<double>());
				}
				else if (i->first == "TEMP")
				{
					temp = i->second.get<double>();
				}
				else if (i->first == "RAM")
				{
					ram = i->second.get<double>();
				}
			}
		}
	}
	return 0;
}
