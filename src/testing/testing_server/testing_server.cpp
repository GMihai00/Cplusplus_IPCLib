#include <iostream>

#include "../common/command_line_parser.hpp"
#include "../common/event_helpers.hpp"

#include "test_server.hpp"

int main(int argc, char* argv[])
{
	utile::command_line_parser cmd_parser(argc, argv);

	HANDLE_START_EVENT(cmd_parser);

	auto server_ip = utile::IP_ADRESS(utile::get_option_or_quit(cmd_parser, "--srv_ip"));
	auto server_port = (utile::PORT)std::stoi(std::string(utile::get_option_or_quit(cmd_parser, "--srv_port")));

	try
	{
		test_server server(server_ip, server_port);

		server.start();

		std::cout << "Server started\n";

		// in case cond var attempt fails and smth is done on the main thread on server side or on another obj
		// PS this should not happen
		//while (true)
		//{
		//	Sleep(500);
		//}

		std::condition_variable cond_infinit_run;
		std::mutex mutext_infinit_run;

		std::unique_lock ulock(mutext_infinit_run);
		cond_infinit_run.wait(ulock);
	}
	catch (const std::exception& err)
	{
		std::cerr << "Failed to start server err: " << err.what();
	}

}