#pragma once

#include "../common/message_types.hpp"

#include "net/client.hpp"

class test_client : public net::client<TestingMessage>
{
public:
	void send_ok_test_message();
	void send_big_data_test_message();
	void send_nok_test_message();
};