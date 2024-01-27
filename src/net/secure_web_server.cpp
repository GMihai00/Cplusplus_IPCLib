#include "secure_web_server.hpp"

namespace net
{
	secure_web_server::secure_web_server(const utile::IP_ADRESS& host, const std::string& cert_file, 
		const std::optional<std::string>& dh_file, const utile::PORT port,
		const uint64_t max_nr_connections, const uint64_t number_threads) 
		: base_web_server(host, port, max_nr_connections, number_threads) 
		, m_ssl_context(boost::asio::ssl::context::tlsv12_server)
	{
		m_build_client_socket_function = [this](boost::asio::io_context& context) {
			return std::make_shared<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>>(context, m_ssl_context);
		};

		set_build_client_socket_function(m_build_client_socket_function);

		m_handshake_function = std::bind(&secure_web_server::handshake, this, std::placeholders::_1, std::placeholders::_2);

		set_handshake_function(m_handshake_function);

		if (dh_file)
			m_ssl_context.use_tmp_dh_file(*dh_file);

		m_ssl_context.use_certificate_chain_file(cert_file);
		m_ssl_context.use_private_key_file(cert_file, boost::asio::ssl::context::pem);

		m_verify_certificate_callback = [](bool preverified, boost::asio::ssl::verify_context& ctx)
			{
				char subject_name[256];
				X509* cert = X509_STORE_CTX_get_current_cert(ctx.native_handle());
				X509_NAME_oneline(X509_get_subject_name(cert), subject_name, 256);
				std::cout << "Verifying " << subject_name << " preverified: " << preverified << "\n";

				return preverified;
			};

		if (m_verify_certificate_callback)
			m_ssl_context.set_verify_callback(m_verify_certificate_callback);
	}

	void secure_web_server::set_verify_certificate_callback(const std::function<bool(bool, boost::asio::ssl::verify_context& ctx)>& verify_certificate_callback)
	{
		assert(verify_certificate_callback);

		m_verify_certificate_callback = verify_certificate_callback;
		m_ssl_context.set_verify_callback(m_verify_certificate_callback);
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
