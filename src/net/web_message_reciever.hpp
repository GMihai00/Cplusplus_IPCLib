#pragma once

#include <iostream>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/bind.hpp>
#include <memory>

#include "http_request.hpp"
#include "http_response.hpp"
#include "../utile/generic_error.hpp"
#include "../utile/finally.hpp"

namespace net
{
	typedef std::function<void(std::shared_ptr<ihttp_message>, utile::web_error)> async_get_callback;

	template <typename T>
	class web_message_reciever
	{
	public:
		web_message_reciever() = delete;

		web_message_reciever(std::shared_ptr<T>& socket)
			: m_socket(socket)
		{
		}

		template <typename R>
		std::pair<std::shared_ptr<R>, utile::web_error> get() noexcept try
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

			auto response = std::make_shared<R>();

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

		template <typename R>
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

			auto message = std::make_shared<R>();

			async_read(message, callback);
		}
		
		void set_socket(std::shared_ptr<T>& socket)
		{
			m_socket = socket;
		}

	private:
		void async_read(std::shared_ptr<ihttp_message> message, async_get_callback& callback) noexcept
		{
			boost::asio::async_read_until(*m_socket, message->get_buffer(), "\r\n\r\n", [this, message, &callback](const boost::system::error_code& error, std::size_t /*bytes_transferred*/) {
				if (error)
				{
					m_waiting_for_message = false;
					if (callback) callback(nullptr, utile::web_error(std::error_code(error.value(), std::generic_category()), error.message()));
				}
				else
				{
					if (!message->build_header_from_data_recieved())
					{
						m_waiting_for_message = false;
						if (callback) callback(nullptr, utile::web_error(std::error_code(error.value(), std::generic_category()), error.message()));
					}
					else
					{
						async_try_to_extract_body(message, callback);
					}
				}
				});
		}

		void try_to_extract_body(std::shared_ptr<ihttp_message> message)
		{
			try_to_extract_body_using_current_lenght(message) || try_to_extract_body_using_transfer_encoding(message) || try_to_extract_body_using_connection_closed(message);
		}

		bool try_to_extract_body_using_current_lenght(std::shared_ptr<ihttp_message> message)
		{
			auto body_lenght = message->get_header_value<uint32_t>("Content-Length");
			if (body_lenght == std::nullopt)
			{
				return false;
			}

			// remove already read characters from size of buffer;
			*body_lenght -= static_cast<uint32_t>(message->get_buffer().size());

			uint32_t available_bytes = 0;
			do
			{
				available_bytes = static_cast<uint32_t>(m_socket->lowest_layer().available());

				if (available_bytes >= *body_lenght)
				{
					boost::asio::read(*m_socket, message->get_buffer(), boost::asio::transfer_exactly(*body_lenght));
					body_lenght.value() = 0;
				}
				else
				{
					*body_lenght -= available_bytes;
					boost::asio::read(*m_socket, message->get_buffer(), boost::asio::transfer_exactly(available_bytes));
				}

			} while (*body_lenght != 0);

			return true;
		}

		bool try_to_extract_body_using_transfer_encoding(std::shared_ptr<ihttp_message> message)
		{
			auto encoding = message->get_header_value<std::string>("Transfer-Encoding");

			if (encoding == std::nullopt || encoding.value() != "chunked")
			{
				return false;
			}

			boost::asio::streambuf streambuf;

			{
				// clear already existing buffer and copy to streambuf

				std::istream source_stream(&(message->get_buffer()));

				std::ostream target_stream(&streambuf);

				target_stream << source_stream.rdbuf();
			}

			std::ostream ostream(&(message->get_buffer()));

			bool should_read_size = true;
			uint16_t buffer_size = 0;

			do
			{
				// search in streambuf for "\r\n", if found skip reading from socket

				auto delimiter = find_delimiter_poz(streambuf, "\r\n");

				if (delimiter == std::nullopt)
				{
					boost::asio::read_until(*m_socket, streambuf, "\r\n");
				}

				delimiter = find_delimiter_poz(streambuf, "\r\n");

				if (delimiter == std::nullopt)
				{
					throw std::runtime_error("Failed to find delimiter, error occured when reading data");
				}

				if (!should_read_size)
				{
					if (*delimiter - 2 != buffer_size)
					{
						throw std::runtime_error("Invalid message body recieved, missing bytes: " + (buffer_size - (*delimiter - 1)));
					}

					copy_buffer_data_to_stream(ostream, streambuf, *delimiter);
				}
				else
				{
					copy_buffer_data_to_number(buffer_size, streambuf, *delimiter);
				}

				should_read_size = 1 - should_read_size;

				streambuf.consume(*delimiter);

			} while (buffer_size == 0 && should_read_size);


			return true;
		}

		bool try_to_extract_body_using_connection_closed(std::shared_ptr<ihttp_message> response)
		{
			auto connection_status = response->get_header_value<std::string>("Connection");

			if (connection_status == std::nullopt || connection_status.value() != "closed")
			{
				return false;
			}


			std::size_t available_bytes = 0;
			do
			{
				available_bytes = m_socket->lowest_layer().available();

				boost::asio::read(*m_socket, response->get_buffer(), boost::asio::transfer_exactly(available_bytes));

			} while (available_bytes != 0);

			return true;
		}


		void async_read_all_remaining_data(const boost::system::error_code& error, std::size_t bytes_transferred, 
			std::shared_ptr<ihttp_message> message, async_get_callback& callback) noexcept
		{
			if (!error)
			{
				if (bytes_transferred > 0)
				{
					boost::asio::async_read(*m_socket, message->get_buffer(),
						boost::asio::transfer_at_least(1), // Read at least 1 byte
						boost::bind(&web_message_reciever::async_read_all_remaining_data,
							this,
							boost::asio::placeholders::error,
							boost::asio::placeholders::bytes_transferred,
							boost::ref(message),
							boost::ref(callback)
						)
					);
				}
				else
				{
					m_waiting_for_message = false;
					message->finalize_message();
					if (callback) callback(message, utile::web_error());
				}
			}
			else
			{
				m_waiting_for_message = false;
				if (callback) callback(nullptr, utile::web_error(std::error_code(error.value(), std::generic_category()), error.message()));
			}
		}

		void async_read_bytes(std::shared_ptr<ihttp_message> message, async_get_callback& callback, std::size_t bytes_remaining) noexcept
		{
			if (bytes_remaining == 0)
			{
				m_waiting_for_message = false;
				message->finalize_message();
				if (callback) callback(message, utile::web_error());
				return;
			}

			boost::asio::async_read(*m_socket, message->get_buffer(), boost::asio::transfer_at_least(1),
				[this, &callback, message, bytes_remaining](const boost::system::error_code& error, std::size_t bytes_transferred) {
					if (error)
					{
						m_waiting_for_message = false;
						if (callback) callback(nullptr, utile::web_error(std::error_code(error.value(), std::generic_category()), error.message()));
					}
					else
					{
						async_read_bytes(message, callback, bytes_remaining - bytes_transferred);
					}
				});
		}

		void async_try_to_extract_body(std::shared_ptr<ihttp_message> message, async_get_callback& callback) noexcept
		{
			async_try_to_extract_body_using_current_lenght(message, callback);
		}

		void async_try_to_extract_body_using_current_lenght(std::shared_ptr<ihttp_message> message, async_get_callback& callback) noexcept
		{
			auto body_lenght = message->get_header_value<size_t>("Content-Length");
			if (body_lenght == std::nullopt)
			{
				async_try_to_extract_body_using_transfer_encoding(message, callback);
				return;
			}

			// remove already read characters from size of buffer;
			*body_lenght -= message->get_buffer().size();

			std::size_t bytes_to_read = (*body_lenght);
			async_read_bytes(message, callback, bytes_to_read);
		}

		void async_read_fragmented_body(std::shared_ptr<ihttp_message> message, async_get_callback& callback, bool should_read_size = true, uint16_t buffer_size = 0) noexcept
		{
			do
			{
				// search in streambuf for "\r\n", if found skip reading from socket

				auto delimiter = find_delimiter_poz(m_request_buff, "\r\n");

				if (delimiter == std::nullopt)
				{
					boost::asio::async_read_until(*m_socket, m_request_buff, "\r\n", [this, &callback, message, should_read_size, buffer_size](const boost::system::error_code& error, std::size_t /*bytes_transferred*/) {
						if (error)
						{
							m_waiting_for_message = false;
							if (callback) callback(nullptr, utile::web_error(std::error_code(error.value(), std::generic_category()), "Failed to read enough data err: " + error.message()));
						}
						else
						{
							async_read_fragmented_body(message, callback, should_read_size, buffer_size);
						}
						});
					return;
				}

				if (!should_read_size)
				{
					if (*delimiter - 2 != buffer_size)
					{
						if (callback) callback(nullptr, utile::web_error(std::error_code(5, std::generic_category()), "Invalid message body recieved, missing bytes: " + (buffer_size - (*delimiter - 1))));
						return;
					}

					std::ostream ostream(&(message->get_buffer()));
					copy_buffer_data_to_stream(ostream, m_request_buff, *delimiter);
				}
				else try
				{
					copy_buffer_data_to_number(buffer_size, m_request_buff, *delimiter);
				}
				catch (const std::exception& err)
				{
					m_waiting_for_message = false;
					if (callback) callback(nullptr, utile::web_error(std::error_code(-1, std::generic_category()), "Invalid fragmented body found err: " + std::string(err.what())));
					return;
				}

				should_read_size = 1 - should_read_size;

				m_request_buff.consume(*delimiter);

			} while (buffer_size == 0 && should_read_size);

			m_waiting_for_message = false;
			message->finalize_message();
			if (callback) callback(message, utile::web_error());
		}

		void async_try_to_extract_body_using_transfer_encoding(std::shared_ptr<ihttp_message> message, async_get_callback& callback) noexcept
		{
			auto encoding = message->get_header_value<std::string>("Transfer-Encoding");

			if (encoding == std::nullopt || encoding.value() != "chunked")
			{
				async_try_to_extract_body_using_connection_closed(message, callback);
				return;
			}

			{
				// clear already existing buffer and copy to streambuf
				std::istream source_stream(&(message->get_buffer()));

				std::ostream target_stream(&m_request_buff);

				target_stream << source_stream.rdbuf();
			}

			async_read_fragmented_body(message, callback);
		}

		void async_try_to_extract_body_using_connection_closed(std::shared_ptr<ihttp_message> message, async_get_callback& callback) noexcept
		{
			auto connection_status = message->get_header_value<std::string>("Connection");

			if (connection_status == std::nullopt || connection_status.value() != "closed")
			{
				// no body found
				m_waiting_for_message = false;
				message->finalize_message();
				if (callback) callback(message, utile::web_error());
			}

			boost::asio::async_read(*m_socket, message->get_buffer(),
				boost::asio::transfer_at_least(1), // Read at least 1 byte
				boost::bind(&web_message_reciever::async_read_all_remaining_data,
					this,
					boost::asio::placeholders::error,
					boost::asio::placeholders::bytes_transferred,
					boost::ref(message),
					boost::ref(callback)
				)
			);
		}

		void copy_buffer_data_to_stream(std::ostream& ostream, boost::asio::streambuf& streambuf, const size_t size)
		{
			const char* data = boost::asio::buffer_cast<const char*>(streambuf.data());

			ostream << std::string(data, size - 2);
		}

		void copy_buffer_data_to_number(uint16_t& buffer_size, boost::asio::streambuf& streambuf, const size_t size)
		{
			const char* data = boost::asio::buffer_cast<const char*>(streambuf.data());

			buffer_size = (uint16_t)std::stoll(std::string(data, size - 2));
		}

		std::optional<size_t> find_delimiter_poz(boost::asio::streambuf& buffer, const std::string& delimiter)
		{
			const char* data = boost::asio::buffer_cast<const char*>(buffer.data());
			std::size_t size = buffer.size();

			std::optional<size_t> rez = std::nullopt;

			for (std::size_t i = 0; i < size - delimiter.size() + 1; ++i) {
				if (std::memcmp(data + i, delimiter.c_str(), delimiter.size()) == 0) {
					return i + delimiter.size();
				}
			}

			return std::nullopt;
		}

	private:
		std::mutex m_mutex;
		std::atomic_bool m_waiting_for_message = false;
		std::shared_ptr<T>& m_socket = nullptr;
		boost::asio::streambuf m_request_buff;
	};
}
