#include "test_client.hpp"

#include <cinttypes>

#include "net/message.hpp"

void test_client::send_ok_test_message()
{
	std::cout << "Sending ok test message\n";

	net::message<TestingMessage> msg;
	msg.m_header.m_type = TestingMessage::TEST;

	msg << true;

	msg << 5;

	send(msg);
}

void test_client::send_nok_test_message()
{
	net::message<TestingMessage> msg;
	msg.m_header.m_type = TestingMessage::NOK_MESSAGE;

	msg << false;
	send(msg);
}

void test_client::send_big_data_test_message()
{
	std::cout << "Sending big data message\n";

	net::message<TestingMessage> msg;
	msg.m_header.m_type = TestingMessage::BIG_DATA;

	std::vector<uint8_t> data(100000, 1);

	msg << data;

	send(msg);
}