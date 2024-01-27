#pragma once

#include "base_web_server.hpp"

#include <boost/asio/ssl.hpp>

namespace net
{

	class secure_web_server : public base_web_server<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>>
	{
	public:
		secure_web_server(const utile::IP_ADRESS& host, const std::string& cert_file, const std::optional<std::string>& dh_file = std::nullopt, const utile::PORT port = 443, const uint64_t max_nr_connections = 1000, const uint64_t number_threads = 4);
		virtual ~secure_web_server() = default;
		
		void set_verify_certificate_callback(const std::function<bool(bool, boost::asio::ssl::verify_context& ctx)>& verify_certificate_callback);
	private:
		void handshake(std::shared_ptr<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>> client_socket, std::function<void(std::shared_ptr<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>>)> callback);

		std::function<std::shared_ptr<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>>(boost::asio::io_context&)> m_build_client_socket_function = nullptr;
		std::function<void(std::shared_ptr<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>>, std::function<void(std::shared_ptr<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>>)>)> m_handshake_function = nullptr;
		boost::asio::ssl::context m_ssl_context;
		std::function<bool(bool, boost::asio::ssl::verify_context& ctx)> m_verify_certificate_callback = nullptr;
	};
}
