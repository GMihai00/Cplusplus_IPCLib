#pragma once

#include <vector>
#include <optional>
#include <string>

namespace utile
{
	class command_line_parser
	{
	private:
		const std::vector<std::string_view> options;
	public:
		command_line_parser() = delete;
		command_line_parser(int argc, char* argv[]);
		~command_line_parser() noexcept = default;

		std::optional<std::string_view> get_option(const std::string_view& option_name) const;
	};
}