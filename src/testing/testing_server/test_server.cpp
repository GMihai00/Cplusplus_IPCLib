#include "test_server.hpp"


test_server::test_server(const utile::IP_ADRESS& host, utile::PORT port) 
	: net::server<TestingMessage>(host, port)
{

}

bool test_server::can_client_connect(std::shared_ptr<net::connection<TestingMessage>> client) noexcept
{
	return true;
}

void test_server::on_client_connect(std::shared_ptr<net::connection<TestingMessage>> client) noexcept
{

	std::cout << "Client with ip: \"" << client->get_ip_adress() << "\" connected\n";
}

void test_server::on_client_disconnect(std::shared_ptr<net::connection<TestingMessage>> client) noexcept
{
	std::cout << "Client with ip: \"" << client->get_ip_adress() << "\" disconnected\n";
}

void test_server::do_idle_work()
{

}

void test_server::on_message(std::shared_ptr<net::connection<TestingMessage>> client, net::message<TestingMessage>& msg) noexcept
{
	std::cout << "Recieved new message of type: " << (int)TestingMessage::TEST << '\n';
	switch (msg.m_header.m_type)
	{
	case TestingMessage::TEST:
		do_idle_work();
		approve_request(client, msg);
		break;
	default:
		break;
	}
}

void test_server::approve_request(std::shared_ptr<net::connection<TestingMessage>> client, net::message<TestingMessage>& msg)
{
	auto reply = msg.build_reply();
	msg.m_header.m_type = TestingMessage::ACK;

	message_client(client, reply);
}
void test_server::reject_request(std::shared_ptr<net::connection<TestingMessage>> client, net::message<TestingMessage>& msg)
{
	auto reply = msg.build_reply();
	msg.m_header.m_type = TestingMessage::NACK;

	message_client(client, reply);
}

