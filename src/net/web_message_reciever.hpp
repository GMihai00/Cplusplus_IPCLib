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
	typedef std::function<void(std::shared_ptr<ihttp_message>, utile::web_error)> async_get_callback;

	class web_message_reciever
	{
	public:
		web_message_reciever() = delete;

		web_message_reciever(std::shared_ptr<boost::asio::ip::tcp::socket> socket);

		template <typename T>
		std::pair<std::shared_ptr<T>, utile::web_error> get() noexcept try
		{
			{
				std::scoped_lock lock(m_mutex);
				if (m_waiting_for_message)
				{
					return { nullptr, REQUEST_ALREADY_ONGOING };
				}
				m_waiting_for_message = true;
			}

			auto final_action = utile::finally([&]() {
				m_waiting_for_message = false;
				});

			auto response = std::make_shared<T>();

			boost::asio::read_until(*m_socket, response->get_buffer(), "\r\n\r\n");

			if (!response->build_header_from_data_recieved())
			{
				return { nullptr, utile::web_error(std::error_code(5, std::generic_category()), "Invalid message header recieved") };
			}

			try_to_extract_body(response);

			response->finalize_message();

			return { response, utile::web_error() };
		}
		catch (const boost::system::system_error& err)
		{
			return { nullptr, utile::web_error(err.code(), err.what()) };
		}
		catch (const std::exception& err)
		{
			return { nullptr, utile::web_error(std::error_code(1000, std::generic_category()), err.what()) };
		}
		catch (...)
		{
			return { nullptr, INTERNAL_ERROR };
		}

		template <typename T>
		void async_get(async_get_callback& callback) noexcept
		{
			{
				std::scoped_lock lock(m_mutex);
				if (m_waiting_for_message)
				{
					if (callback) callback(nullptr, REQUEST_ALREADY_ONGOING);
					return;
				}
				m_waiting_for_message = true;
			}

			auto message = std::make_shared<T>();

			async_read(message, callback);
		}
		
	private:
		void async_read(std::shared_ptr<ihttp_message> message, async_get_callback& callback) noexcept;

		void try_to_extract_body(std::shared_ptr<ihttp_message> message);
		bool try_to_extract_body_using_current_lenght(std::shared_ptr<ihttp_message> message);
		bool try_to_extract_body_using_transfer_encoding(std::shared_ptr<ihttp_message> message);
		bool try_to_extract_body_using_connection_closed(std::shared_ptr<ihttp_message> message);


		void async_read_all_remaining_data(const boost::system::error_code& error, std::size_t bytes_transferred, std::shared_ptr<ihttp_message> message, async_get_callback& callback) noexcept;

		void async_read_bytes(std::shared_ptr<ihttp_message> message, async_get_callback& callback, std::size_t bytes_remaining) noexcept;
		void async_try_to_extract_body(std::shared_ptr<ihttp_message> message, async_get_callback& callback) noexcept;
		void async_try_to_extract_body_using_current_lenght(std::shared_ptr<ihttp_message> message, async_get_callback& callback) noexcept;
		void async_read_fragmented_body(std::shared_ptr<ihttp_message> message, async_get_callback& callback, bool should_read_size = true, uint16_t buffer_size = 0) noexcept;
		void async_try_to_extract_body_using_transfer_encoding(std::shared_ptr<ihttp_message> message, async_get_callback& callback) noexcept;
		void async_try_to_extract_body_using_connection_closed(std::shared_ptr<ihttp_message> message, async_get_callback& callback) noexcept;

	private:
		std::mutex m_mutex;
		std::atomic_bool m_waiting_for_message = false;
		std::shared_ptr<boost::asio::ip::tcp::socket> m_socket;
		boost::asio::streambuf m_request_buff;
	};
}
