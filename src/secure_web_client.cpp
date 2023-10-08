#include "secure_web_client.hpp"
#include "utile/finally.hpp"
#include <boost/bind.hpp>

namespace net
{
	namespace details
	{
		void copy_buffer_data_to_stream(std::ostream& ostream, boost::asio::streambuf& streambuf)
		{
			const char* data = boost::asio::buffer_cast<const char*>(streambuf.data());
			std::size_t size = streambuf.size();

			ostream << std::string(data, size - 2);
		}

		void copy_buffer_data_to_number(uint16_t& buffer_size, boost::asio::streambuf& streambuf)
		{
			const char* data = boost::asio::buffer_cast<const char*>(streambuf.data());
			std::size_t size = streambuf.size();

			buffer_size = (uint16_t)std::stoll(std::string(data, size - 2));
		}
	}
	secure_web_client::secure_web_client(const std::function<bool(bool, boost::asio::ssl::verify_context& ctx)>& verify_certificate_callback) :
		m_ssl_context(boost::asio::ssl::context::sslv23),
		m_socket(m_io_service, m_ssl_context),
		m_resolver(m_io_service),
		m_verify_certificate_callback(verify_certificate_callback),
		m_idle_work(m_io_service)
	{
		m_ssl_context.set_default_verify_paths();
		if (m_verify_certificate_callback != nullptr)
		{
			m_ssl_context.set_verify_mode(boost::asio::ssl::verify_peer);
			m_ssl_context.set_verify_callback(m_verify_certificate_callback);
		}

		m_io_service.run();
	}

	secure_web_client::~secure_web_client()
	{
		if (m_socket.lowest_layer().is_open())
		{
			disconnect();
		}
	}

	bool secure_web_client::connect(const std::string& url)
	{
		{
			std::scoped_lock lock(m_mutex);
			if (m_socket.lowest_layer().is_open())
			{
				return false;
			}
		}

		boost::asio::ip::tcp::resolver::query query(url, "https");
		boost::asio::connect(m_socket.lowest_layer(), m_resolver.resolve(query));
		m_socket.lowest_layer().set_option(boost::asio::ip::tcp::no_delay(true));

		m_socket.set_verify_mode(boost::asio::ssl::verify_peer);
		m_socket.set_verify_callback(boost::asio::ssl::rfc2818_verification("host.name"));
		m_socket.handshake(boost::asio::ssl::stream<boost::asio::ip::tcp::socket>::client);
		
		m_host = query.host_name();

		return true;
	}

	void secure_web_client::disconnect()
	{
		std::scoped_lock lock(m_mutex);
		if (m_socket.lowest_layer().is_open())
		{
			m_socket.shutdown();
			m_socket.lowest_layer().close();
		}
	}

	void secure_web_client::try_to_extract_body(std::shared_ptr<http_response> response, bool async) noexcept try
	{
		try_to_extract_body_using_current_lenght(response, async) || try_to_extract_body_using_transfer_encoding(response, async) || try_to_extract_body_using_connection_closed(response, async);
	} 
	catch (const std::exception& err)
	{
		std::cout << "Failed to extract body err: " << err.what();
	}

	bool secure_web_client::try_to_extract_body_using_current_lenght(std::shared_ptr<http_response> response, bool async)
	{
		auto body_lenght = response->get_header_value<uint32_t>("Content-Length");
		if (body_lenght == std::nullopt)
		{
			return false;
		}

		if (async)
		{
			boost::asio::async_read(m_socket, response->get_buffer(), boost::asio::transfer_exactly(*body_lenght),
				[] (const boost::system::error_code& error, std::size_t /*bytes_transferred*/) {
					if (error) {
						throw std::runtime_error("Failed to read data err: " + error.value());
					}
				});
		}
		else
		{
			if (auto size = boost::asio::read(m_socket, response->get_buffer(), boost::asio::transfer_exactly(*body_lenght)); size != *body_lenght)
			{
				throw std::runtime_error("Failed to read data missing bytes: " + (*body_lenght - size));
			}
		}

		return true;
	}

	bool secure_web_client::try_to_extract_body_using_transfer_encoding(std::shared_ptr<http_response> response, bool async)
	{
		auto encoding = response->get_header_value<std::string>("Transfer-Encoding");

		if (encoding == std::nullopt || encoding.value() != "Chunked")
		{
			return false;
		}

		std::ostream ostream(&(response->get_buffer()));

		boost::asio::streambuf streambuf;

		bool should_read_size = true;
		uint16_t buffer_size = 0;

		do
		{
			if (async)
			{
				boost::asio::async_read_until(m_socket, response->get_buffer(), "\r\n", [](const boost::system::error_code& error, std::size_t /*bytes_transferred*/) {
					if (error) {
						throw std::runtime_error("Failed to read enough data err: " + error.message());
					}
					});
			}
			else
			{
				boost::asio::read_until(m_socket, streambuf, "\r\n");
			}

			if (!should_read_size)
			{
				if (streambuf.size() - 2 != buffer_size)
				{
					throw std::runtime_error("Invalid message body recieved, missing bytes: " + (buffer_size - (streambuf.size() - 2)));
				}

				details::copy_buffer_data_to_stream(ostream, streambuf);
			}
			else
			{
				details::copy_buffer_data_to_number(buffer_size, streambuf);
			}

			should_read_size = 1 - should_read_size;

			streambuf.consume(streambuf.size());

		} while (buffer_size == 0 && should_read_size);


		return true;
	}

	void read_all_remaining_data(const boost::system::error_code& error, std::size_t bytesRead, boost::asio::ssl::stream<boost::asio::ip::tcp::socket>& socket, boost::asio::streambuf& buffer) {
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

	bool secure_web_client::try_to_extract_body_using_connection_closed(std::shared_ptr<http_response> response, bool async)
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
			boost::asio::read(m_socket, response->get_buffer());
		}
		catch (...)
		{

		}

		return true;
	}

	std::shared_ptr<http_response> secure_web_client::send(http_request& request)
	{
		{
			std::scoped_lock lock(m_mutex);
			if (m_waiting_for_request || !m_socket.lowest_layer().is_open())
			{
				return nullptr;
			}
			m_waiting_for_request = true;
		}

		auto final_action = utile::finally([&]() {
			m_waiting_for_request = false;
			});

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

	std::future<void> secure_web_client::async_write(const std::string& data)
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

	std::future<std::shared_ptr<http_response>> secure_web_client::async_read()
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

	std::future<std::shared_ptr<http_response>> secure_web_client::send_async(http_request& request)
	{
		std::promise<std::shared_ptr<http_response>> promise;

		auto shared_promise = std::make_shared<std::promise<std::shared_ptr<http_response>>>(std::move(promise));

		{
			std::scoped_lock lock(m_mutex);
			if (m_waiting_for_request || !m_socket.lowest_layer().is_open())
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

} // namespace net