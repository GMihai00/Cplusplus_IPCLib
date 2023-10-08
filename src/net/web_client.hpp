#pragma once

#include <iostream>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <future>
#include <memory>

#include "http_request.hpp"
#include "http_response.hpp"

namespace net
{
	class web_client
	{
	public:
		web_client();
		~web_client();

		bool connect(const std::string& url) noexcept;
		void disconnect();

		// timeouts to be added in here

		std::shared_ptr<http_response> send(http_request& request);
		std::future<std::shared_ptr<http_response>> send_async(http_request& request);

	private:

		std::future<void> async_write(const std::string& data);
		std::future<std::shared_ptr<http_response>> async_read();

		void try_to_extract_body(std::shared_ptr<http_response> response, bool async = false) noexcept;
		bool try_to_extract_body_using_current_lenght(std::shared_ptr<http_response> response, bool async);
		bool try_to_extract_body_using_transfer_encoding(std::shared_ptr<http_response> response, bool async);
		bool try_to_extract_body_using_connection_closed(std::shared_ptr<http_response> response, bool async);

		boost::asio::io_service m_io_service;
		boost::asio::io_context::work m_idle_work;
		boost::asio::ip::tcp::socket m_socket;
		boost::asio::ip::tcp::resolver m_resolver;
		std::string m_host{};
		std::mutex m_mutex;
		std::thread m_thread_context;
		std::atomic_bool m_waiting_for_request;
	};

} // namespace net