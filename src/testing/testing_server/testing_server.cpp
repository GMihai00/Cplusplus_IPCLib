#include <iostream>

#include "../common/command_line_parser.hpp"

#include "test_server.hpp"


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

	auto server_ip = utile::IP_ADRESS(get_option_or_quit(cmd_parser, "--srv_ip"));
	auto server_port = (utile::PORT)std::stoi(std::string(get_option_or_quit(cmd_parser, "--srv_port")));

	try
	{
		test_server server(server_ip, server_port);

		server.start();
	}
	catch (const std::exception& err)
	{
		std::cerr << "Failed to start server err: " << err.what();
	}

}