#pragma once

#include "base_web_client.hpp"

namespace net
{
	class web_client : public base_web_client<boost::asio::ip::tcp::socket>
	{
	public:
		web_client();
		virtual ~web_client() = default;

	};
}