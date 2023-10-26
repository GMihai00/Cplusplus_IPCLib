#pragma once

/*
#include <boost/asio.hpp>
#include <boost/asio/ts/buffer.hpp>
#include <boost/asio/ts/internet.hpp>

#include <system_error>
#include <optional>

#include "http_request.hpp"

namespace net
{
	class web_server
	{
	public:
		web_server();
		~web_server();

		std::error_code start();
		std::error_code stop();
	private:
		void wait_for_client_connection() noexcept;

		boost::asio::ip::tcp::endpoint m_endpoint;
		std::optional<boost::asio::ip::tcp::acceptor> m_connection_accepter = std::nullopt;
		boost::asio::io_context m_context;

        // boost::asio::ip::tcp::socket socket instead of connection

        virtual bool can_client_connect([[maybe_unused]] std::shared_ptr<connection<T>> client) noexcept
        {
            return true;
        }

        virtual void on_client_connect([[maybe_unused]] std::shared_ptr<connection<T>> client) noexcept
        {

        }

        virtual void on_client_disconnect([[maybe_unused]] std::shared_ptr<connection<T>> client) noexcept
        {
        }

        // task can be handle in a async matter to think about a way to make this happen maybe create another sepparate class
        // maybe instead I could have a send_async and on_async_message and have it call on_message and que up futures inside  
        // will have to add apart from queue a set of pending tasks or smth alike
        virtual void on_message_async([[maybe_unused]] std::shared_ptr<connection<T>> client, [[maybe_unused]] net::http_request& req) noexcept
        {
            // TO DO, TO BE DISCUSSED IF I CAN JUST HAVE IT RUN ON_MESSAGE INSTEAD
        }

        virtual void on_message([[maybe_unused]] std::shared_ptr<connection<T>> client, [[maybe_unused]] net::http_request& req) noexcept
        {
        }
	};
} */