#include <iostream>
#include <map>

#include "../common/command_line_parser.hpp"
#include "../common/event_helpers.hpp"

#include "utile/timer.hpp"
#include "test_client.hpp"

typedef std::function<void(test_client&, const utile::IP_ADRESS&, const utile::PORT, const uint16_t timeout)> actionfn;

void succes_exit()
{
	exit(0);
}

void signal_error_event()
{
	std::cout << "FAILURE OCCURED\n";
	HANDLE h_fail_event = OpenEventW(EVENT_MODIFY_STATE, FALSE, L"Global\\TestFailed");

	if (h_fail_event != NULL)
	{
		SetEvent(h_fail_event);

		CloseHandle(h_fail_event);
	}
	else
	{
		std::cerr << "Failure happened but fail event not found!!!";
	}

	// just for debugging for now
	Sleep(10000);
	exit(ERROR_INTERNAL_ERROR);
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
			signal_error_event();
		}

		if (ans.value().m_msg.m_header.m_type == TestingMessage::NACK)
		{
			std::cerr << "Message should of been valid but was found invalid";
			signal_error_event();
		}
	}

}

void big_data_run(test_client& client, const utile::IP_ADRESS& srv_ip, const utile::PORT srv_port, const uint16_t timeout)
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
		client.send_big_data_test_message();

		auto ans = client.wait_for_answear(5000);

		if (ans == std::nullopt)
		{
			std::cerr << "Test failed timeout reciving answear";
			signal_error_event();
		}

		auto& msg = ans.value().m_msg;
		if (msg.m_header.m_type == TestingMessage::NACK)
		{
			std::cerr << "Message should of been valid but was found invalid";
			signal_error_event();
		}

		std::vector<uint8_t> data(msg.size());

		// std::cout << "MESSAGE SIZE RECIEVED: " << data.size() << std::endl;

		msg >> data;

		int nr_diferent_values = 0;
		size_t last_poz = 0;
		for (size_t it = 1; it < data.size(); it++)
		{
			if (static_cast<uint8_t>(data[it] - 1) != data[it - 1])
			{
				nr_diferent_values++;
				last_poz = it;
			}
		}

		if (nr_diferent_values)
		{
			std::cout << "FOUND DIFFERNT VALUES NR: " << nr_diferent_values << std::endl;
			std::cout << "LAST CHAR PAIR FOUND: " << (int)data[last_poz] << " " << (int)data[last_poz - 1];
			// Sleep(10000);
			signal_error_event();
		}
	}

}

const std::map<std::string, actionfn> ACTION_MAP =
{
	{ "test", ok_test_run },
	{ "big_data", big_data_run }
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
	
	HANDLE_START_EVENT(cmd_parser);

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
		signal_error_event();
	}

	return 0;
}

