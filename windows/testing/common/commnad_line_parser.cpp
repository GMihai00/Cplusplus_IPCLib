#include "command_line_parser.hpp"


namespace utile
{
	command_line_parser::command_line_parser(int argc, char* argv[]) :
		options(argv + 1, argv + argc)
	{
	}

	std::optional<std::string_view> command_line_parser::get_option(const std::string_view& option_name) const
	{
		std::optional<std::string_view> option = {};
		bool found = false;
		for (const auto& arg : options)
		{
			if (found)
			{
				option = arg;
				break;
			}

			if (arg == option_name)
			{
				found = true;
				continue;
			}
		}

		return option;
	}

	std::string_view get_option_or_quit(utile::command_line_parser& cmd_parser, std::string name)
	{
		auto maybe_option = cmd_parser.get_option(name);

		if (maybe_option == std::nullopt)
			exit(1168);

		return maybe_option.value();
	}

}