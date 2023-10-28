#include "web_message_dispatcher.hpp"

#include "../utile/finally.hpp"

namespace net
{
	web_message_dispatcher::web_message_dispatcher(std::shared_ptr<boost::asio::ip::tcp::socket> socket) : m_socket(socket)
	{
		assert(socket);
	}

	utile::web_error web_message_dispatcher::send(const http_request& request) noexcept try
	{
		{
			std::scoped_lock lock(m_mutex);
			if (m_waiting_for_request)
			{
				return REQUEST_ALREADY_ONGOING;
			}
			m_waiting_for_request = true;
		}

		auto final_action = utile::finally([&]() {
			m_waiting_for_request = false;
			});

		boost::asio::streambuf request_buff;
		std::ostream request_stream(&request_buff);
		request_stream << request.to_string();

		boost::asio::write(*m_socket, request_buff);

		return utile::web_error();
	}
	catch (const boost::system::system_error& err)
	{
		return utile::web_error(err.code(), err.what());
	}
	catch (const std::exception& err)
	{
		return utile::web_error(std::error_code(1000 ,std::generic_category()), err.what());
	}
	catch (...)
	{
		return INTERNAL_ERROR;
	}

	utile::web_error web_message_dispatcher::reply(const http_response& request) noexcept try
	{
		{
			std::scoped_lock lock(m_mutex);
			if (m_waiting_for_request)
			{
				return REQUEST_ALREADY_ONGOING;
			}
			m_waiting_for_request = true;
		}

		auto final_action = utile::finally([&]() {
			m_waiting_for_request = false;
			});

		boost::asio::streambuf request_buff;
		std::ostream request_stream(&request_buff);
		request_stream << request.to_string();

		boost::asio::write(*m_socket, request_buff);

		return utile::web_error();
	}
	catch (const boost::system::system_error& err)
	{
		return utile::web_error(err.code(), err.what());
	}
	catch (const std::exception& err)
	{
		return utile::web_error(std::error_code(1000, std::generic_category()), err.what());
	}
	catch (...)
	{
		return INTERNAL_ERROR;
	}

	void web_message_dispatcher::send_async(const http_request& request, async_send_callback& callback) noexcept
	{
		{
			std::unique_lock<std::mutex> lock(m_mutex);
			if (m_waiting_for_request)
			{
				lock.unlock();

				if (callback) callback(REQUEST_ALREADY_ONGOING);
				return;
			}
			if (m_request_buff.size() != 0) m_request_buff.consume(m_request_buff.size());
			m_waiting_for_request = true;
		}

		std::ostream request_stream(&m_request_buff);
		request_stream << request.to_string();

		async_write(callback);
	}

	void web_message_dispatcher::reply_async(const http_response& request, async_send_callback& callback) noexcept
	{
		{
			std::unique_lock<std::mutex> lock(m_mutex);
			if (m_waiting_for_request)
			{
				lock.unlock();

				if (callback) callback(REQUEST_ALREADY_ONGOING);
				return;
			}
			if (m_request_buff.size() != 0) m_request_buff.consume(m_request_buff.size());
			m_waiting_for_request = true;
		}

		std::ostream request_stream(&m_request_buff);
		request_stream << request.to_string();

		async_write(callback);
	}

	void web_message_dispatcher::async_write(async_send_callback& callback) noexcept
	{
		boost::asio::async_write(*m_socket, m_request_buff, [this, &callback](const boost::system::error_code& error, std::size_t bytes_transferred) {
			if (!error)
			{
				if (m_request_buff.size() == 0)
				{
					m_waiting_for_request = false;
					if (callback) callback(utile::web_error());
				}
				else
				{
					async_write(callback);
				}
			}
			else
			{
				m_waiting_for_request = false;
				if (callback) callback(utile::web_error(std::error_code(error.value(), std::generic_category()), error.message()));
			}
			});
	}
	
}
