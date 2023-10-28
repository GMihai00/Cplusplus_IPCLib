#include "web_message_reciever.hpp"

#include <iostream>
#include <boost/bind.hpp>

namespace net
{
	namespace details
	{
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
	}


	web_message_reciever::web_message_reciever(std::shared_ptr<boost::asio::ip::tcp::socket> socket) : m_socket(socket)
	{
		assert(socket);
	}

	// !m_socket->is_open() facute pe controller


	void web_message_reciever::async_read(std::shared_ptr<ihttp_message> message, async_get_callback& callback) noexcept
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

	void web_message_reciever::try_to_extract_body(std::shared_ptr<ihttp_message> message)
	{
		try_to_extract_body_using_current_lenght(message) || try_to_extract_body_using_transfer_encoding(message) || try_to_extract_body_using_connection_closed(message);
	}

	bool web_message_reciever::try_to_extract_body_using_current_lenght(std::shared_ptr<ihttp_message> message)
	{
		auto body_lenght = message->get_header_value<uint32_t>("Content-Length");
		if (body_lenght == std::nullopt)
		{
			return false;
		}

		// remove already read characters from size of buffer;
		*body_lenght -= message->get_buffer().size();

		std::size_t available_bytes = 0;
		do
		{
			available_bytes = m_socket->available();

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

	bool web_message_reciever::try_to_extract_body_using_transfer_encoding(std::shared_ptr<ihttp_message> message)
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

			auto delimiter = details::find_delimiter_poz(streambuf, "\r\n");

			if (delimiter == std::nullopt)
			{
				boost::asio::read_until(*m_socket, streambuf, "\r\n");
			}

			delimiter = details::find_delimiter_poz(streambuf, "\r\n");

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

				details::copy_buffer_data_to_stream(ostream, streambuf, *delimiter);
			}
			else
			{
				details::copy_buffer_data_to_number(buffer_size, streambuf, *delimiter);
			}

			should_read_size = 1 - should_read_size;

			streambuf.consume(*delimiter);

		} while (buffer_size == 0 && should_read_size);


		return true;
	}

	bool web_message_reciever::try_to_extract_body_using_connection_closed(std::shared_ptr<ihttp_message> response)
	{
		auto connection_status = response->get_header_value<std::string>("Connection");

		if (connection_status == std::nullopt || connection_status.value() != "closed")
		{
			return false;
		}


		std::size_t available_bytes = 0;
		do
		{
			available_bytes = m_socket->available();

			boost::asio::read(*m_socket, response->get_buffer(), boost::asio::transfer_exactly(available_bytes));

		} while (available_bytes != 0);

		return true;
	}

	void web_message_reciever::async_read_all_remaining_data(const boost::system::error_code& error, std::size_t bytes_transferred, std::shared_ptr<ihttp_message> message, async_get_callback& callback) noexcept
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

	void web_message_reciever::async_read_bytes(std::shared_ptr<ihttp_message> message, async_get_callback& callback, std::size_t bytes_remaining) noexcept
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

	void web_message_reciever::async_try_to_extract_body(std::shared_ptr<ihttp_message> message, async_get_callback& callback) noexcept
	{
		async_try_to_extract_body_using_current_lenght(message, callback);
	}

	void web_message_reciever::async_try_to_extract_body_using_current_lenght(std::shared_ptr<ihttp_message> message, async_get_callback& callback) noexcept
	{
		auto body_lenght = message->get_header_value<uint32_t>("Content-Length");
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

	void web_message_reciever::async_read_fragmented_body(std::shared_ptr<ihttp_message> message, async_get_callback& callback, bool should_read_size, uint16_t buffer_size) noexcept
	{
		do
		{
			// search in streambuf for "\r\n", if found skip reading from socket

			auto delimiter = details::find_delimiter_poz(m_request_buff, "\r\n");

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
				details::copy_buffer_data_to_stream(ostream, m_request_buff, *delimiter);
			}
			else try
			{
				details::copy_buffer_data_to_number(buffer_size, m_request_buff, *delimiter);
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

	void web_message_reciever::async_try_to_extract_body_using_transfer_encoding(std::shared_ptr<ihttp_message> message, async_get_callback& callback) noexcept
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

	void web_message_reciever::async_try_to_extract_body_using_connection_closed(std::shared_ptr<ihttp_message> message, async_get_callback& callback) noexcept
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

}