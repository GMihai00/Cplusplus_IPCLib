#pragma once

#include <iostream>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <memory>

#include "http_request.hpp"
#include "http_response.hpp"
#include "../utile/generic_error.hpp"
#include "../utile/finally.hpp"

namespace net
{
	typedef std::function<void(utile::web_error)> async_send_callback;

	class web_message_dispatcher
	{
	public:
		web_message_dispatcher() = delete;

		web_message_dispatcher(std::shared_ptr<boost::asio::ip::tcp::socket> socket);

		template <typename T>
		utile::web_error send(const T& message) noexcept try
		{
			{
				std::scoped_lock lock(m_mutex);
				if (m_waiting_for_message)
				{
					return REQUEST_ALREADY_ONGOING;
				}
				m_waiting_for_message = true;
			}

			auto final_action = utile::finally([&]() {
				m_waiting_for_message = false;
				});

			boost::asio::streambuf request_buff;
			std::ostream request_stream(&request_buff);
			request_stream << message.to_string();

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

		template <typename T>
		void send_async(const T& message, async_send_callback& callback) noexcept
		{
			{
				std::unique_lock<std::mutex> lock(m_mutex);
				if (m_waiting_for_message)
				{
					lock.unlock();

					if (callback) callback(REQUEST_ALREADY_ONGOING);
					return;
				}
				if (m_request_buff.size() != 0) m_request_buff.consume(m_request_buff.size());
				m_waiting_for_message = true;
			}

			std::ostream request_stream(&m_request_buff);
			request_stream << message.to_string();

			async_write(callback);
		}
		
	private:
		void async_write(async_send_callback& callback) noexcept;

		boost::asio::streambuf m_request_buff;
		std::mutex m_mutex;
		std::atomic_bool m_waiting_for_message = false;
		std::shared_ptr<boost::asio::ip::tcp::socket> m_socket;
	};
}