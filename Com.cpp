#include "Com.h"

// Read json form first { to last }
uint16_t get_data(uint8_t *buffer, size_t size)
{
	uint16_t buffer_index = 0;
	int level = 0;

	do {
		int c = getchar_timeout_us(1000000000);
		if (c == PICO_ERROR_TIMEOUT)
			return 0;

		if (c == '{')
			level++;
		else if (c == '}')
			level--;

		buffer[buffer_index++] = (c & 0xFF);
	} while (level>0 && (buffer_index < size - 1));

    buffer[buffer_index] = 0;

    return buffer_index;
}

