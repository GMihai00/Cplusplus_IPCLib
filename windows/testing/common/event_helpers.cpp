#include "event_helpers.hpp"
#include <Windows.h>
#include <iostream>

#include "type_converters.hpp"

namespace utile
{
	int handle_start_event_if_present(command_line_parser& cmd_parser)
	{
		auto maybe_start_event_guid = cmd_parser.get_option("--start_event_guid");
		if (maybe_start_event_guid != std::nullopt)
		{
			HANDLE h_start_event = OpenEventW(EVENT_MODIFY_STATE, FALSE, utf8_to_utf16(std::string(maybe_start_event_guid.value())).c_str());

			if (h_start_event == NULL)
			{
				std::cerr << "Start event name exists, but start event can't be found";
				return 5;
			}

			SetEvent(h_start_event);

			CloseHandle(h_start_event);
		}

		return 0;
	}
}