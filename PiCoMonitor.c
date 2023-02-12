#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#define BUFFER_LENGTH 10

// Format buffer
// CPU1-16 * 10%
//10 10 10 10 10 10 10 10 10 10 10 10 10 10 10 10 


uint16_t get_message(uint8_t *buffer) {
  uint16_t buffer_index= 0;
  while (true) {
    int c = getchar_timeout_us(1000);
    if (c != PICO_ERROR_TIMEOUT && buffer_index < BUFFER_LENGTH-1) {
      buffer[buffer_index++] = (c & 0xFF);
    } else {
      break;
    }
  }
  return buffer_index;
}


int main() 
{
	stdio_init_all();

	// Init Wifi
	if (cyw43_arch_init()) {
        printf("WiFi init failed");
        return -1;
    }

	int led = 0; 
	uint8_t buffer[BUFFER_LENGTH];
	memset(buffer,0,BUFFER_LENGTH);

	while (true) {
		// Read next message
		uint16_t len = get_message(buffer);
		buffer[len] = 0;
		if (len > 0)
		{
			printf("%s\n", buffer);
			cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led );
			led = (led)?0:1;
		}
	}
	return 0;
}
