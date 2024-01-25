#include "secure_web_server.hpp"

namespace net
{


	secure_web_server::secure_web_server(const utile::IP_ADRESS& host, const std::string& cert_file, 
		const std::optional<std::string>& dh_file, const utile::PORT port,
		const uint64_t max_nr_connections, const uint64_t number_threads) 
		: base_web_server(host, port, max_nr_connections, number_threads) 
		, m_ssl_context(boost::asio::ssl::context::tlsv12_server)
	{
		if (dh_file)
			m_ssl_context.use_tmp_dh_file(*dh_file);
		
		m_ssl_context.use_certificate_chain_file(cert_file);
		m_ssl_context.use_private_key_file(cert_file, boost::asio::ssl::context::pem);

		m_build_client_socket_function = [this](boost::asio::io_context& context) {
			return std::make_shared<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>>(context, m_ssl_context);
		};

		m_handshake_function = std::bind(&secure_web_server::handshake, this, std::placeholders::_1, std::placeholders::_2);

		set_build_client_socket_function(m_build_client_socket_function);
		set_handshake_function(m_handshake_function);

	}

	void secure_web_server::handshake(std::shared_ptr<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>> client_socket,
		std::function<void(std::shared_ptr<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>>)> callback)
	{
		client_socket->async_handshake(boost::asio::ssl::stream<boost::asio::ip::tcp::socket>::handshake_type::server,
			[client_socket, callback](const boost::system::error_code& err) {
				if (!err) 
				{
					callback(client_socket);
					err.value();
				}
				else
				{
#ifdef DEBUG
					std::cerr << "Failed to establish handshake err: " << err.message() << std::endl;
#endif // DEBUG
				}
			});
	}
}
