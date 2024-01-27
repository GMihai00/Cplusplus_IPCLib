#include "web_client.hpp"
#include "../utile/finally.hpp"
#include <boost/bind/bind.hpp>

namespace net
{
	web_client::web_client() : base_web_client<boost::asio::ip::tcp::socket>(), m_resolver(m_io_service)
	{
		set_socket(std::make_shared<boost::asio::ip::tcp::socket>(std::move(boost::asio::ip::tcp::socket(m_io_service))));
	}

	bool web_client::connect(const std::string& url, const std::optional<std::string>& port) noexcept try
	{
		{
			std::scoped_lock lock(m_mutex);
			if (m_socket->lowest_layer().is_open())
			{
				return false;
			}
		}

		std::string string_port = "http";

		if (port != std::nullopt)
		{
			string_port = *port;
		}

		boost::asio::ip::tcp::resolver::query query(url, string_port);
		boost::asio::connect(m_socket->lowest_layer(), m_resolver.resolve(query));
		m_socket->lowest_layer().set_option(boost::asio::ip::tcp::no_delay(true));

		m_host = url;

		return true;
	}
	catch (const std::exception& err)
	{
		std::cerr << "Failed to connect to server, err: " << err.what();
		return false;
	}
} // namespace net