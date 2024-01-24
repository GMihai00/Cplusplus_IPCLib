#pragma once

#include "base_web_server.hpp"

namespace net
{

	class web_server : public base_web_server<boost::asio::ip::tcp::socket>
	{
	public:

		web_server(const utile::IP_ADRESS& host, const utile::PORT port = 80, const uint64_t max_nr_connections = 1000, const uint64_t number_threads = 4);
		virtual ~web_server() = default;
	};
} 
