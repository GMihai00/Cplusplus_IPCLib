#include "web_client.hpp"
#include "../utile/finally.hpp"
#include <boost/bind.hpp>

namespace net
{
	web_client::web_client() :
		m_socket(std::make_shared<boost::asio::ip::tcp::socket>(std::move(boost::asio::ip::tcp::socket(m_io_service)))),
		m_resolver(m_io_service),
		m_idle_work(m_io_service),
		m_controller(m_socket)
	{
		m_thread_context = std::thread([this]() { m_io_service.run(); });
	}

	web_client::~web_client()
	{
		if (m_socket->is_open())
		{
			disconnect();
		}

		m_io_service.stop();

		if (m_thread_context.joinable())
			m_thread_context.join();
	}

	bool web_client::connect(const std::string& url, const std::optional<utile::PORT>& port) noexcept try
	{
		{
			std::scoped_lock lock(m_mutex);
			if (m_socket->is_open())
			{
				return false;
			}
		}

		std::string string_port = "http";

		if (port != std::nullopt)
		{
			string_port = std::to_string(*port);
		}

		boost::asio::ip::tcp::resolver::query query(url, string_port);
		boost::asio::connect(*m_socket, m_resolver.resolve(query));
		m_socket->set_option(boost::asio::ip::tcp::no_delay(true));
		
		m_host = url;

		return true;
	}
	catch (const std::exception& err)
	{
		std::cerr << "Failed to connect to server, err: " << err.what();
		return false;
	}

	void web_client::disconnect()
	{
		std::scoped_lock lock(m_mutex);
		if (m_socket->is_open())
		{
			m_socket->close();
		}
	}
	
	std::pair<std::shared_ptr<http_response>, utile::web_error> web_client::send(http_request&& request, const uint16_t timeout)
	{
		request.set_host(m_host);

		return m_controller.send(std::move(request), timeout);
	}


	void web_client::send_async(http_request&& request, async_get_callback& callback) noexcept
	{
		request.set_host(m_host);

		m_controller.send_async(std::move(request), callback);
	}

} // namespace net