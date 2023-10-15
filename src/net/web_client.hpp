#pragma once

#include <iostream>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <future>
#include <memory>

#include "http_request.hpp"
#include "http_response.hpp"
#include "../utile/timer.hpp"

namespace net
{
	typedef std::function<void(std::shared_ptr<http_response>, std::string)> async_send_callback;

	class web_client
	{
	public:
		web_client();
		~web_client();

		bool connect(const std::string& url) noexcept;
		// to be called if you want to cancel async request
		void disconnect();

		std::shared_ptr<http_response> send(http_request& request, const uint16_t timeout = 0);

		void send_async(http_request& request, async_send_callback& callback) noexcept;

		bool last_request_timedout() const;
	private:
		void try_to_extract_body(std::shared_ptr<http_response> response) noexcept;
		bool try_to_extract_body_using_current_lenght(std::shared_ptr<http_response> response);
		bool try_to_extract_body_using_transfer_encoding(std::shared_ptr<http_response> response);
		bool try_to_extract_body_using_connection_closed(std::shared_ptr<http_response> response);

		void async_write(async_send_callback& callback) noexcept;
		void async_read(async_send_callback& callback) noexcept;

		void async_read_all_remaining_data(const boost::system::error_code& error, std::size_t bytes_transferred, std::shared_ptr<http_response> response, async_send_callback& callback) noexcept;

		void async_try_to_extract_body(std::shared_ptr<http_response> response, async_send_callback& callback) noexcept;
		void async_try_to_extract_body_using_current_lenght(std::shared_ptr<http_response> response, async_send_callback& callback) noexcept;
		void async_try_to_extract_body_using_transfer_encoding(std::shared_ptr<http_response> response, async_send_callback& callback) noexcept;
		void async_try_to_extract_body_using_connection_closed(std::shared_ptr<http_response> response, async_send_callback& callback) noexcept;

		boost::asio::io_service m_io_service;
		boost::asio::io_context::work m_idle_work;
		boost::asio::ip::tcp::socket m_socket;
		boost::asio::ip::tcp::resolver m_resolver;
		std::string m_host{};
		std::mutex m_mutex;
		std::thread m_thread_context;
		std::atomic_bool m_waiting_for_request;
		std::atomic_bool m_timedout = false;
		std::function<void()> m_timeout_callback;
		boost::asio::streambuf m_request_buff;
		std::shared_ptr<utile::observer<>> m_timeout_observer;
	};

} // namespace net