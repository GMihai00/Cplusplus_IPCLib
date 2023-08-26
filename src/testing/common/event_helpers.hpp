#pragma once

#include "command_line_parser.hpp"

namespace utile
{
	int handle_start_event_if_present(command_line_parser& cmd_parser);
}

#define HANDLE_START_EVENT(cmd_parser)									     \
    do {																	 \
       if (auto ret = utile::handle_start_event_if_present(cmd_parser); ret) \
	   {																     \
		   return ret;														 \
	   }																	 \
    } while (0)





