#pragma once

#include <iostream>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <memory>

#include "http_request.hpp"
#include "http_response.hpp"
#include "../utile/generic_error.hpp"

namespace net
{
	typedef std::function<void(utile::web_error)> async_send_callback;

	class web_message_dispatcher
	{
	public:
		web_message_dispatcher() = delete;

		web_message_dispatcher(std::shared_ptr<boost::asio::ip::tcp::socket> socket);

		utile::web_error send(const http_request& request) noexcept;
		utile::web_error reply(const http_request& request) noexcept;

		void send_async(const http_request& request, async_send_callback& callback) noexcept;

		void reply_async(const http_response& request, async_send_callback& callback) noexcept;
		
	private:
		void async_write(async_send_callback& callback) noexcept;

		boost::asio::streambuf m_request_buff;
		std::mutex m_mutex;
		std::atomic_bool m_waiting_for_request = false;
		std::shared_ptr<boost::asio::ip::tcp::socket> m_socket;
	};
}