#include <iostream>
#include <map>

#include "../common/command_line_parser.hpp"
#include "utile/timer.hpp"
#include "test_client.hpp"

typedef std::function<void(test_client&, const utile::IP_ADRESS&, const utile::PORT, const uint16_t timeout)> actionfn;

void succes_exit()
{
	exit(0);
}

void ok_test_run(test_client& client, const utile::IP_ADRESS& srv_ip, const utile::PORT srv_port, const uint16_t timeout)
{
	try
	{
		if (!client.connect(srv_ip, srv_port))
		{
			std::cerr << "Failed to connect to the server";
			exit(5);
		}
	}
	catch (const std::exception& err)
	{
		std::cerr << err.what();
		exit(5);
	}


	std::function<void()> quit_fct = std::bind(succes_exit);
	std::shared_ptr<utile::observer<>> obs_exit = std::make_shared<utile::observer<>>(quit_fct);

	utile::timer<> timer_exit(timeout);

	if (timeout)
	{
		timer_exit.subscribe(obs_exit);
		timer_exit.resume();
	}

	while (true)
	{
		client.send_ok_test_message();

		auto ans = client.wait_for_answear(5000);

		if (ans == std::nullopt)
		{
			std::cerr << "Test failed timeout reciving answear";
			exit(ERROR_TIMEOUT);
		}

		if (ans.value().m_msg.m_header.m_type == TestingMessage::NACK)
		{
			std::cerr << "Message should of been valid but was found invalid";
			exit(ERROR_INVALID_CATEGORY);
		}
	}

}

const std::map<std::string, actionfn> ACTION_MAP =
{
	{ "test", ok_test_run }
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
	auto server_port = (utile::PORT) std::stoi(std::string(get_option_or_quit(cmd_parser, "--srv_port")));
	auto timeout = (uint16_t) std::stoi(std::string(get_option_or_quit(cmd_parser, "--timeout")));

	if (auto it = ACTION_MAP.find(action); it != ACTION_MAP.end())
	{
		test_client client;
		it->second(client, server_ip, server_port, timeout);
	}
	else
	{
		// SHOULD SIGNAL ERROR EVENT I QUESS
		std::cerr << "Invalid action found: " << action;
		return ERROR_NOT_FOUND;
	}

	return 0;
}

