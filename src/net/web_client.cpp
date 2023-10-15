#include "web_client.hpp"
#include "../utile/finally.hpp"
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
	web_client::web_client() :
		m_socket(m_io_service),
		m_resolver(m_io_service),
		m_idle_work(m_io_service)
	{
		m_timeout_callback = [this]() { if (this) { m_timedout = true; disconnect(); } };
		m_timeout_observer = std::make_shared<utile::observer<>>(m_timeout_callback);

		m_thread_context = std::thread([this]() { m_io_service.run(); });
	}

	web_client::~web_client()
	{
		if (m_socket.is_open())
		{
			disconnect();
		}

		m_io_service.stop();

		if (m_thread_context.joinable())
			m_thread_context.join();
	}

	bool web_client::connect(const std::string& url) noexcept try
	{
		{
			std::scoped_lock lock(m_mutex);
			if (m_socket.is_open())
			{
				return false;
			}
		}

		boost::asio::ip::tcp::resolver::query query(url, "http");
		boost::asio::connect(m_socket, m_resolver.resolve(query));
		m_socket.set_option(boost::asio::ip::tcp::no_delay(true));

		m_host = url;

		m_timedout = false;

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
		if (m_socket.is_open())
		{
			m_socket.close();
		}
	}

	void web_client::try_to_extract_body(std::shared_ptr<http_response> response) noexcept try
	{
		try_to_extract_body_using_current_lenght(response) || try_to_extract_body_using_transfer_encoding(response) || try_to_extract_body_using_connection_closed(response);
	}
	catch (const std::exception& err)
	{
		std::cerr << "Failed to extract body err: " << err.what();
	}

	void web_client::async_try_to_extract_body(std::shared_ptr<http_response> response, async_send_callback& callback) noexcept
	{
		async_try_to_extract_body_using_current_lenght(response, callback);
	}

	bool web_client::try_to_extract_body_using_current_lenght(std::shared_ptr<http_response> response)
	{
		auto body_lenght = response->get_header_value<uint32_t>("Content-Length");
		if (body_lenght == std::nullopt)
		{
			return false;
		}

		// remove already read characters from size of buffer;
		*body_lenght -= response->get_buffer().size();

		// need to add a timeout mechanism
		std::size_t available_bytes = 0;
		do
		{
			available_bytes = m_socket.available();

			if (available_bytes >= *body_lenght)
			{
				boost::asio::read(m_socket, response->get_buffer(), boost::asio::transfer_exactly(*body_lenght));
				body_lenght.value() = 0;
			}
			else
			{
				*body_lenght -= available_bytes;
				boost::asio::read(m_socket, response->get_buffer(), boost::asio::transfer_exactly(available_bytes));
			}

		} while (*body_lenght != 0);

		return true;
	}

	// TO REPAIR
	void web_client::async_try_to_extract_body_using_current_lenght(std::shared_ptr<http_response> response, async_send_callback& callback) noexcept
	{
		auto body_lenght = response->get_header_value<uint32_t>("Content-Length");
		if (body_lenght == std::nullopt)
		{
			async_try_to_extract_body_using_transfer_encoding(response, callback);
			return;
		}

		// remove already read characters from size of buffer;
		*body_lenght -= response->get_buffer().size();

		do
		{
			std::size_t available_bytes = m_socket.available();

			if (available_bytes >= *body_lenght)
			{
				boost::asio::async_read(m_socket, response->get_buffer(), boost::asio::transfer_exactly(*body_lenght),
					[](const boost::system::error_code& error, std::size_t /*bytes_transferred*/) {
						if (error) {
							throw std::runtime_error("Failed to read data err: " + error.value());
						}
					});
				body_lenght.value() = 0;
			}
			else
			{
				*body_lenght -= available_bytes;

				boost::asio::async_read(m_socket, response->get_buffer(), boost::asio::transfer_exactly(available_bytes),
					[](const boost::system::error_code& error, std::size_t /*bytes_transferred*/) {
						if (error) {
							throw std::runtime_error("Failed to read data err: " + error.value());
						}
					});
			}
		} while (*body_lenght != 0);
	
	}

	bool web_client::try_to_extract_body_using_transfer_encoding(std::shared_ptr<http_response> response)
	{
		auto encoding = response->get_header_value<std::string>("Transfer-Encoding");

		if (encoding == std::nullopt || encoding.value() != "chunked")
		{
			return false;
		}

		boost::asio::streambuf streambuf;

		{
			// clear already existing buffer and copy to streambuf
			std::size_t bytes_to_copy = response->get_buffer().size();

			std::istream source_stream(&(response->get_buffer()));

			std::ostream target_stream(&streambuf);

			target_stream << source_stream.rdbuf();
		}

		std::ostream ostream(&(response->get_buffer()));

		bool should_read_size = true;
		uint16_t buffer_size = 0;

		do
		{
			// search in streambuf for "\r\n", if found skip reading from socket

			auto delimiter = details::find_delimiter_poz(streambuf, "\r\n");

			if (delimiter == std::nullopt)
			{
				boost::asio::read_until(m_socket, streambuf, "\r\n");
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

	// TO REPAIR
	void web_client::async_try_to_extract_body_using_transfer_encoding(std::shared_ptr<http_response> response, async_send_callback& callback) noexcept
	{
		auto encoding = response->get_header_value<std::string>("Transfer-Encoding");

		if (encoding == std::nullopt || encoding.value() != "chunked")
		{
			async_try_to_extract_body_using_connection_closed(response, callback);
			return;
		}

		boost::asio::streambuf streambuf;

		{
			// clear already existing buffer and copy to streambuf
			std::size_t bytes_to_copy = response->get_buffer().size();

			std::istream source_stream(&(response->get_buffer()));

			std::ostream target_stream(&streambuf);

			target_stream << source_stream.rdbuf();
		}

		std::ostream ostream(&(response->get_buffer()));

		bool should_read_size = true;
		uint16_t buffer_size = 0;

		do
		{
			// search in streambuf for "\r\n", if found skip reading from socket

			auto delimiter = details::find_delimiter_poz(streambuf, "\r\n");

			if (delimiter == std::nullopt)
			{
				boost::asio::async_read_until(m_socket, streambuf, "\r\n", [](const boost::system::error_code& error, std::size_t /*bytes_transferred*/) {
					if (error) {
						throw std::runtime_error("Failed to read enough data err: " + error.message());
					}
					});
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
	}

	bool web_client::try_to_extract_body_using_connection_closed(std::shared_ptr<http_response> response)
	{
		auto connection_status = response->get_header_value<std::string>("Connection");

		if (connection_status == std::nullopt || connection_status.value() != "closed")
		{
			return false;
		}


		std::size_t available_bytes = 0;
		do
		{
			available_bytes = m_socket.available();
			
			boost::asio::read(m_socket, response->get_buffer(), boost::asio::transfer_exactly(available_bytes));
				
		}
		while(available_bytes != 0);

		return true;
	}

	void web_client::async_read_all_remaining_data(const boost::system::error_code& error, std::size_t bytes_transferred, std::shared_ptr<http_response> response, async_send_callback& callback) noexcept
	{
		if (!error) 
		{
			if (bytes_transferred > 0)
			{

				boost::asio::async_read(m_socket, response->get_buffer(),
					boost::asio::transfer_at_least(1), // Read at least 1 byte
					boost::bind(&web_client::async_read_all_remaining_data,
						this,
						boost::asio::placeholders::error,
						boost::asio::placeholders::bytes_transferred,
						boost::ref(response),
						boost::ref(callback)
					)
				);
			}
			else
			{
				response->finalize_message();
				if (callback) callback(response, "Succes");
				m_waiting_for_request = false;

			}
		}
		else
		{
			if (callback) callback(nullptr, error.message());
			m_waiting_for_request = false;
		}
	}

	void web_client::async_try_to_extract_body_using_connection_closed(std::shared_ptr<http_response> response, async_send_callback& callback) noexcept
	{
		auto connection_status = response->get_header_value<std::string>("Connection");

		if (connection_status == std::nullopt || connection_status.value() != "closed")
		{
			response->finalize_message();
			if (callback) callback(response, "Succes");
			m_waiting_for_request = false;
		}

		boost::asio::async_read(m_socket, response->get_buffer(),
			boost::asio::transfer_at_least(1), // Read at least 1 byte
			boost::bind(&web_client::async_read_all_remaining_data,
				this,
				boost::asio::placeholders::error,
				boost::asio::placeholders::bytes_transferred,
				boost::ref(response),
				boost::ref(callback)
			)
		);
	}

	std::shared_ptr<http_response> web_client::send(http_request& request, const uint16_t timeout)
	{
		{
			std::scoped_lock lock(m_mutex);
			if (m_waiting_for_request || !m_socket.is_open())
			{
				return nullptr;
			}
			m_waiting_for_request = true;
		}

		auto final_action = utile::finally([&]() {
			m_waiting_for_request = false;
			});

		utile::timer<> cancel_timer(0);

		cancel_timer.subscribe(m_timeout_observer);

		if (timeout)
		{
			cancel_timer.reset(timeout);
			cancel_timer.resume();
		}

		request.set_host(m_host);

		boost::asio::streambuf request_buff;
		std::ostream request_stream(&request_buff);
		request_stream << request.to_string();

		boost::asio::write(m_socket, request_buff);

		auto response = std::make_shared<http_response>();

		boost::asio::read_until(m_socket, response->get_buffer(), "\r\n\r\n");

		if (!response->build_header_from_data_recieved())
		{
			std::cerr << "Invalid message header recieved";
			return nullptr;
		}

		try_to_extract_body(response);

		response->finalize_message();

		return response;
	}

	void web_client::async_write(async_send_callback& callback) noexcept
	{

		boost::asio::async_write(m_socket, m_request_buff, [this, &callback](const boost::system::error_code& error, std::size_t bytes_transferred) {
			if (!error)
			{

				if (bytes_transferred > m_request_buff.size())
				{
					m_waiting_for_request = false;
					if (callback) callback(nullptr, "Wrote more bytes then expected, boost asio problem");
				}

				m_request_buff.consume(bytes_transferred);

				if (m_request_buff.size() == 0)
				{
					async_read(callback);
				}
				else
				{
					async_write(callback);
				}
			}
			else
			{
				m_waiting_for_request = false;
				if (callback) callback(nullptr, error.message());
			}
		});
	}

	void web_client::async_read(async_send_callback& callback) noexcept
	{
		auto response = std::make_shared<http_response>();

		boost::asio::async_read_until(m_socket, response->get_buffer(), "\r\n\r\n", [this, response, &callback](const boost::system::error_code& error, std::size_t /*bytes_transferred*/) {
			if (error) {
				if (callback) callback(nullptr, error.message());
				m_waiting_for_request = false;
			}
			else
			{
				if (!response->build_header_from_data_recieved())
				{
					if (callback) callback(nullptr, "Invalid message header recieved");
					m_waiting_for_request = false;
				}
				else
				{
					async_try_to_extract_body(response, callback);
				}
			}
		});
	}

	void web_client::send_async(http_request& request, async_send_callback& callback) noexcept
	{
		{
			std::unique_lock<std::mutex> lock(m_mutex);
			if (m_waiting_for_request || !m_socket.is_open())
			{
				lock.unlock();
				if (callback) callback(nullptr, "Request already ongoing");
				return;
			}
			if (m_request_buff.size() != 0) m_request_buff.consume(m_request_buff.size());
			m_waiting_for_request = true;
		}

		request.set_host(m_host);

		std::ostream request_stream(&m_request_buff);
		request_stream << request.to_string();

		async_write(callback);
	}


	bool web_client::last_request_timedout() const
	{
		return m_timedout;
	}
} // namespace net