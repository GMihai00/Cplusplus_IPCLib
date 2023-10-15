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

	void web_client::try_to_extract_body(std::shared_ptr<http_response> response, bool async) noexcept try
	{
		try_to_extract_body_using_current_lenght(response, async) || try_to_extract_body_using_transfer_encoding(response, async) || try_to_extract_body_using_connection_closed(response, async);
	}
	catch (const std::exception& err)
	{
		std::cerr << "Failed to extract body err: " << err.what();
	}

	bool web_client::try_to_extract_body_using_current_lenght(std::shared_ptr<http_response> response, bool async)
	{
		auto body_lenght = response->get_header_value<uint32_t>("Content-Length");
		if (body_lenght == std::nullopt)
		{
			return false;
		}

		// remove already read characters from size of buffer;
		*body_lenght -= response->get_buffer().size();

		if (async)
		{
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
		else
		{
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
		}

		return true;
	}

	bool web_client::try_to_extract_body_using_transfer_encoding(std::shared_ptr<http_response> response, bool async)
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
				if (async)
				{
					boost::asio::async_read_until(m_socket, streambuf, "\r\n", [](const boost::system::error_code& error, std::size_t /*bytes_transferred*/) {
						if (error) {
							throw std::runtime_error("Failed to read enough data err: " + error.message());
						}
						});
				}
				else
				{
					boost::asio::read_until(m_socket, streambuf, "\r\n");
				}
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

	void read_all_remaining_data(const boost::system::error_code& error, std::size_t bytesRead, boost::asio::ip::tcp::socket& socket, boost::asio::streambuf& buffer) {
		if (!error) {
			if (bytesRead > 0) {

				boost::asio::async_read(socket, buffer,
					boost::asio::transfer_at_least(1), // Read at least 1 byte
					boost::bind(read_all_remaining_data,
						boost::asio::placeholders::error,
						boost::asio::placeholders::bytes_transferred,
						boost::ref(socket),
						boost::ref(buffer)
					)
				);
			}
			else {
			}
		}
	}

	bool web_client::try_to_extract_body_using_connection_closed(std::shared_ptr<http_response> response, bool async)
	{
		auto connection_status = response->get_header_value<std::string>("Connection");

		if (connection_status == std::nullopt || connection_status.value() != "closed")
		{
			return false;
		}

		if (async)
		{
			auto& buffer = response->get_buffer();

			boost::asio::async_read(m_socket, buffer,
				boost::asio::transfer_at_least(1), // Read at least 1 byte
				boost::bind(read_all_remaining_data,
					boost::asio::placeholders::error,
					boost::asio::placeholders::bytes_transferred,
					boost::ref(m_socket),
					boost::ref(buffer)
				)
			);
		}
		else try
		{
			std::size_t available_bytes = 0;
			do
			{
				available_bytes = m_socket.available();
			
				boost::asio::read(m_socket, response->get_buffer(), boost::asio::transfer_exactly(available_bytes));
				
			}
			while(available_bytes != 0);
		}
		catch (...)
		{

		}

		return true;
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

	std::future<void> web_client::async_write(const std::string& data)
	{
		std::promise<void> promise;

		auto shared_promise = std::make_shared<std::promise<void>>(std::move(promise));

		boost::asio::streambuf request_buff;
		std::ostream request_stream(&request_buff);
		request_stream << data;

		boost::asio::async_write(m_socket, request_buff, [shared_promise](const boost::system::error_code& error, std::size_t /*bytes_transferred*/) {
			if (!error) {
				shared_promise->set_value();
			}
			else {
				shared_promise->set_exception(std::make_exception_ptr(std::runtime_error(error.message())));
			}
			});

		return shared_promise->get_future();
	}

	std::future<std::shared_ptr<http_response>> web_client::async_read()
	{
		std::promise<std::shared_ptr<http_response>> promise;

		auto shared_promise = std::make_shared<std::promise<std::shared_ptr<http_response>>>(std::move(promise));

		auto response = std::make_shared<http_response>();

		boost::asio::async_read_until(m_socket, response->get_buffer(), "\r\n\r\n", [shared_promise, &response](const boost::system::error_code& error, std::size_t /*bytes_transferred*/) {
			if (error) {
				shared_promise->set_exception(std::make_exception_ptr(std::runtime_error(error.message())));
			}
			});

		if (!response->build_header_from_data_recieved())
		{
			shared_promise->set_exception(std::make_exception_ptr(std::runtime_error("Invalid message header recieved")));
		}
		else
		{
			try_to_extract_body(response, true);

			response->finalize_message();

			shared_promise->set_value(response);
		}

		return shared_promise->get_future();
	}

	std::future<std::shared_ptr<http_response>> web_client::send_async(http_request& request)
	{
		// after calling async boost:asio operations I'm not guaranted to have finished the operation...
		// they are running on same thread so need to change logic
		// Split into 2 operations: I'll just set a signal in here and wait for it to end in the main thread basically whenever I need it
		// there will be callbacks after callbacks after callbacks..
		throw std::runtime_error("REMOVED FOR NOW, NOT YET WORKING AS EXPECTED");

		std::promise<std::shared_ptr<http_response>> promise;

		auto shared_promise = std::make_shared<std::promise<std::shared_ptr<http_response>>>(std::move(promise));

		{
			std::scoped_lock lock(m_mutex);
			if (m_waiting_for_request || !m_socket.is_open())
			{
				shared_promise->set_exception(std::make_exception_ptr(std::runtime_error("Request already ongoing")));
				return shared_promise->get_future();
			}
			m_waiting_for_request = true;
		}


		auto final_action = utile::finally([&]() {
			m_waiting_for_request = false;
			});

		try {
			request.set_host(m_host);
			async_write(request.to_string()).get();
		}
		catch (const std::exception& e) {
			shared_promise->set_exception(std::make_exception_ptr(e));
			return shared_promise->get_future();
		}

		shared_promise.reset();

		return async_read();
	}


	bool web_client::last_request_timedout() const
	{
		return m_timedout;
	}
} // namespace net