#include "Com.h"
#include "picojson.h"

#include <algorithm>

// Read json form first { to last }
uint16_t get_data(char *buffer, size_t size)
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

bool decode_data(char *buffer, size_t bufsize, MonitorData& data)
{
	picojson::value v;
	std::string err = picojson::parse(v, std::string(buffer));
	if (! err.empty()) return false;
	if (! v.is<picojson::object>()) return false;

	const auto& obj = v.get<picojson::object>();
	for (auto&& i = obj.begin(); i != obj.end(); ++i) 
	{
		if (i->first == "CPU" && i->second.is<picojson::array>())
		{
			const auto& arr = i->second.get<picojson::array>();
			for (auto&& j = arr.begin(); j != arr.end(); ++j) 
				data.cpu_percent.push_back(j->get<double>());
		}
		else if (i->first == "TEMP")
		{
			data.temp = i->second.get<double>();
		}
		else if (i->first == "RAM")
		{
			data.ram = i->second.get<double>();
		}
	}

	return true;
}
