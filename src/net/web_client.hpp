#pragma once

#include <iostream>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <future>
#include <memory>

#include "http_request.hpp"
#include "http_response.hpp"
#include "../utile/timer.hpp"
#include "web_message_controller.hpp"
#include "../utile/data_types.hpp"

namespace net
{
	class web_client
	{
	public:
		web_client();
		~web_client();

		bool connect(const std::string& url, const std::optional<utile::PORT>& port = std::nullopt) noexcept;
		// to be called if you want to cancel async request
		void disconnect();

		std::pair<std::shared_ptr<http_response>, utile::web_error> send(http_request&& request, const uint16_t timeout = 0);

		void send_async(http_request&& request, async_get_callback& callback) noexcept;
	private:

		boost::asio::io_service m_io_service;
		boost::asio::io_context::work m_idle_work;
		std::shared_ptr<boost::asio::ip::tcp::socket> m_socket;
		boost::asio::ip::tcp::resolver m_resolver;
		std::string m_host{};
		std::mutex m_mutex;
		std::thread m_thread_context;
		web_message_controller m_controller;
	};

} // namespace net