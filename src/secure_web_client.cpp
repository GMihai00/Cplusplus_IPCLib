#include "secure_web_client.hpp"
#include "utile/finally.hpp"

namespace net
{
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

	std::shared_ptr<https_response> secure_web_client::send(const https_request& request)
	{
		{
			std::scoped_lock lock(m_mutex);
			if (m_waiting_for_request)
			{
				return nullptr;
			}
		}

		m_waiting_for_request = true;
		auto final_action = utile::finally([&]() {
			m_waiting_for_request = false;
			});

		boost::asio::streambuf request_buff;
		std::ostream request_stream(&request_buff);
		request_stream << request.to_string();

		boost::asio::write(m_socket, request_buff);

		auto response = std::make_shared<https_response>();
		boost::asio::read_until(m_socket, response->get_buffer(), "\r\n");

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

	std::future<std::shared_ptr<https_response>> secure_web_client::async_read()
	{
		std::promise<std::shared_ptr<https_response>> promise;

		auto shared_promise = std::make_shared<std::promise<std::shared_ptr<https_response>>>(std::move(promise));

		auto response = std::make_shared<https_response>();

		boost::asio::async_read_until(m_socket, response->get_buffer(), "\r\n", [shared_promise, &response](const boost::system::error_code& error, std::size_t /*bytes_transferred*/) {
			if (!error) {

				shared_promise->set_value(response);
			}
			else {
				shared_promise->set_exception(std::make_exception_ptr(std::runtime_error(error.message())));
			}
			});


		return shared_promise->get_future();
	}

	std::future<std::shared_ptr<https_response>> secure_web_client::send_async(const https_request& request)
	{
		std::promise<std::shared_ptr<https_response>> promise;

		auto shared_promise = std::make_shared<std::promise<std::shared_ptr<https_response>>>(std::move(promise));

		{
			std::scoped_lock lock(m_mutex);
			if (m_waiting_for_request)
			{
				shared_promise->set_exception(std::make_exception_ptr(std::runtime_error("Request already ongoing")));
				return shared_promise->get_future();
			}
		}

		m_waiting_for_request = true;
		auto final_action = utile::finally([&]() {
			m_waiting_for_request = false;
			});

		try {
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