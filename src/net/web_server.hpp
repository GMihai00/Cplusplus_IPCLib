#pragma once

#include <boost/asio.hpp>
#include <boost/asio/ts/buffer.hpp>
#include <boost/asio/ts/internet.hpp>

#include <system_error>

#include "../utile/thread_safe_queue.hpp"

#include "http_request.hpp"
#include "web_message_controller.hpp"
#include "../utile/data_types.hpp"
#include "../utile/generic_error.hpp"

namespace net
{
	class web_server
	{
	public:

		web_server(const utile::IP_ADRESS& host, utile::PORT port, const uint64_t max_nr_connections = 1000);
		~web_server();

		utile::web_error start();
        utile::web_error stop();
	private:

		void wait_for_client_connection() noexcept;

		boost::asio::ip::tcp::endpoint m_endpoint;
		boost::asio::ip::tcp::acceptor m_connection_accepter;
		boost::asio::io_context m_context;
        std::mutex m_mutex;
        
        utile::thread_safe_queue<uint64_t> m_available_connection_ids;

        std::map<uint64_t, web_message_controller> m_clients_controllers;

        // boost::asio::ip::tcp::socket socket instead of connection

        virtual bool can_client_connect([[maybe_unused]] const std::shared_ptr<boost::asio::ip::tcp::socket> client) noexcept
        {
            return true;
        }

        virtual void on_client_connect([[maybe_unused]] const std::shared_ptr<boost::asio::ip::tcp::socket> client) noexcept
        {

        }

        virtual void on_client_disconnect([[maybe_unused]] const std::shared_ptr<boost::asio::ip::tcp::socket> client) noexcept
        {
        }

        virtual void on_message([[maybe_unused]] const std::shared_ptr<boost::asio::ip::tcp::socket> client, [[maybe_unused]] net::http_request& req) noexcept
        {
        }
	};
} 