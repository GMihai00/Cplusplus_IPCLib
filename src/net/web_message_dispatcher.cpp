#include "web_message_dispatcher.hpp"

namespace net
{
	web_message_dispatcher::web_message_dispatcher(std::shared_ptr<boost::asio::ip::tcp::socket> socket) : m_socket(socket)
	{
		assert(socket);
	}

	void web_message_dispatcher::async_write(async_send_callback& callback) noexcept
	{
		boost::asio::async_write(*m_socket, m_request_buff, [this, &callback](const boost::system::error_code& error, const std::size_t /*bytes_transferred*/) {
			if (!error)
			{
				if (m_request_buff.size() == 0)
				{
					m_waiting_for_message = false;
					if (callback) callback(utile::web_error());
				}
				else
				{
					async_write(callback);
				}
			}
			else
			{
				m_waiting_for_message = false;
				if (callback) callback(utile::web_error(std::error_code(error.value(), std::generic_category()), error.message()));
			}
			});
	}
	
}
