#include <iostream>
#include <map>

#include "../common/command_line_parser.hpp"
#include "test_client.hpp"


typedef std::function<void(test_client&, const utile::IP_ADRESS&, const utile::PORT, const int timeout)> actionfn;

const std::map<std::string, actionfn> ACTION_MAP =
{

};

std::string_view get_option_or_quit(utile::command_line_parser& cmd_parser, std::string name)
{
	auto maybe_option = cmd_parser.get_option(name);

	if (maybe_option == std::nullopt)
		exit(ERROR_NOT_FOUND);

	return maybe_option.value();
}

int main(int argc, char* argv[])
{
    utile::command_line_parser cmd_parser(argc, argv);
	
	auto action = std::string(get_option_or_quit(cmd_parser, "--cmd"));
	auto server_ip = utile::IP_ADRESS(get_option_or_quit(cmd_parser, "--srv_ip"));
	auto server_port = std::stoi(std::string(get_option_or_quit(cmd_parser, "--srv_port")));
	auto timeout = std::stoi(std::string(get_option_or_quit(cmd_parser, "--timeout")));

	if (auto it = ACTION_MAP.find(action); it != ACTION_MAP.end())
	{
		test_client client;
		it->second(client, server_ip, server_port, timeout);
	}
	else
	{
		std::cerr << "Invalid action found: " << action;
		return ERROR_NOT_FOUND;
	}

	return 0;
}

