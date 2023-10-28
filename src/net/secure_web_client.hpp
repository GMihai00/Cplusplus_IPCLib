//#pragma once
//
//#include <iostream>
//#include <boost/asio.hpp>
//#include <boost/asio/ssl.hpp>
//#include <future>
//#include <memory>
//
//#include "http_request.hpp"
//#include "http_response.hpp"
//#include "../utile/timer.hpp"
//
//namespace net
//{
//	class secure_web_client
//	{
//	public:
//		secure_web_client(const std::function<bool(bool, boost::asio::ssl::verify_context& ctx)>& verify_certificate_callback = nullptr);
//		~secure_web_client();
//
//		bool connect(const std::string& url) noexcept;
//		void disconnect();
//
//		 // timeouts to be added in here
//
//		std::shared_ptr<http_response> send(http_request& request, const uint16_t timeout = 0);
//		std::future<std::shared_ptr<http_response>> send_async(http_request& request);
//
//	private:
//
//		std::future<void> async_write(const std::string& data);
//		std::future<std::shared_ptr<http_response>> async_read();
//
//		void try_to_extract_body(std::shared_ptr<http_response> response, bool async = false) noexcept;
//		bool try_to_extract_body_using_current_lenght(std::shared_ptr<http_response> response, bool async);
//		bool try_to_extract_body_using_transfer_encoding(std::shared_ptr<http_response> response, bool async);
//		bool try_to_extract_body_using_connection_closed(std::shared_ptr<http_response> response, bool async);
//
//		boost::asio::io_service m_io_service;
//		boost::asio::io_context::work m_idle_work;
//		boost::asio::ssl::context m_ssl_context;
//		boost::asio::ssl::stream<boost::asio::ip::tcp::socket> m_socket;
//		boost::asio::ip::tcp::resolver m_resolver;
//		std::string m_host{};
//		std::mutex m_mutex;
//		std::thread m_thread_context;
//		std::atomic_bool m_waiting_for_request;
//		std::function<bool(bool, boost::asio::ssl::verify_context& ctx)> m_verify_certificate_callback;
//		std::atomic_bool m_timedout = false;
//		std::function<void()> m_timeout_callback;
//		std::shared_ptr<utile::observer<>> m_timeout_observer;
//	};
//
//} // namespace net