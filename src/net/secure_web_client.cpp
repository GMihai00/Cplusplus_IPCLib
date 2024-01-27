#include "secure_web_client.hpp"

namespace net
{
	secure_web_client::secure_web_client(const std::vector<std::string>& pem_files, const std::function<bool(bool, boost::asio::ssl::verify_context& ctx)>& verify_certificate_callback) :
		base_web_client(),
		m_ssl_context(boost::asio::ssl::context::tlsv12_client),
		m_verify_certificate_callback(verify_certificate_callback),
		m_resolver(m_io_service)
	{
		set_socket(std::make_shared<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>>(
			std::move(boost::asio::ssl::stream<boost::asio::ip::tcp::socket>(m_io_service, m_ssl_context))));

		m_ssl_context.set_default_verify_paths();
		m_ssl_context.set_verify_mode(boost::asio::ssl::verify_peer);

		for (const auto& pem_file : pem_files)
			m_ssl_context.load_verify_file(pem_file);

		if (m_verify_certificate_callback != nullptr)
		{
			m_ssl_context.set_verify_callback(m_verify_certificate_callback);
		}
	}

	void secure_web_client::set_verify_certificate_callback(const std::function<bool(bool, boost::asio::ssl::verify_context& ctx)>& verify_certificate_callback)
	{
		assert(verify_certificate_callback);

		m_verify_certificate_callback = verify_certificate_callback;
		m_ssl_context.set_verify_callback(m_verify_certificate_callback);
	}

	void secure_web_client::load_pem_files(const std::vector<std::string>& pem_files)
	{
		for (const auto& pem_file : pem_files)
			m_ssl_context.load_verify_file(pem_file);
	}

	bool secure_web_client::connect(const std::string& url, const  std::optional<std::string>& port) noexcept try
	{
		{
			std::scoped_lock lock(m_mutex);
			if (m_socket->lowest_layer().is_open())
			{
				return false;
			}
		}

		std::string string_port = "https";

		if (port != std::nullopt)
		{
			string_port = *port;
		}

		boost::asio::ip::tcp::resolver::query query(url, string_port);
		boost::asio::connect(m_socket->lowest_layer(), m_resolver.resolve(query));
		m_socket->lowest_layer().set_option(boost::asio::ip::tcp::no_delay(true));

		if (m_verify_certificate_callback == nullptr)
		{
			m_ssl_context.set_verify_callback(boost::asio::ssl::rfc2818_verification(url));
		}

		m_socket->handshake(boost::asio::ssl::stream<boost::asio::ip::tcp::socket>::client);

		m_host = url;

		return true;
	}
	catch (const std::exception& err)
	{
		std::cerr << "Failed to connect to server, err: " << err.what();
		return false;
	}

} // namespace net