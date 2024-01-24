#include "secure_web_server.hpp"

namespace net
{


	secure_web_server::secure_web_server(const utile::IP_ADRESS& host, const utile::PORT port, 
		const uint64_t max_nr_connections, const uint64_t number_threads) 
		: base_web_server(host, port, max_nr_connections, number_threads) 
		, m_ssl_context(boost::asio::ssl::context::tlsv12_server)
	{
		// TO DO
		/*m_ssl_context.use_certificate_file("server.crt", boost::asio::ssl::context::pem);
		m_ssl_context.use_private_key_file("server.key", boost::asio::ssl::context::pem);*/

		m_build_client_socket_function = [this](boost::asio::io_context& context) {
			return std::make_shared<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>>(context, m_ssl_context);
		};

		set_build_client_socket_function(m_build_client_socket_function);
	}
}
