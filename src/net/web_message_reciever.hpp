#pragma once

#include <iostream>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <memory>

#include "http_request.hpp"
#include "http_response.hpp"
#include "../utile/generic_error.hpp"

namespace net
{
	typedef std::function<void(std::shared_ptr<http_response>, utile::web_error)> async_get_callback;

	typedef std::function<void(std::shared_ptr<http_request>, utile::web_error)> async_req_callback;

	class web_message_reciever
	{
	public:
		web_message_reciever() = delete;

		web_message_reciever(std::shared_ptr<boost::asio::ip::tcp::socket> socket);

		std::pair<std::shared_ptr<http_response>, utile::web_error> get_response() noexcept;
		void async_get_response(async_get_callback& callback) noexcept;

		std::pair<std::shared_ptr<http_request>, utile::web_error> get_request() noexcept;
		void async_get_request(async_req_callback& callback) noexcept;
		
	private:
		void async_read(std::shared_ptr<http_response> response, async_get_callback& callback) noexcept;

		void try_to_extract_body(std::shared_ptr<http_response> response);
		bool try_to_extract_body_using_current_lenght(std::shared_ptr<http_response> response);
		bool try_to_extract_body_using_transfer_encoding(std::shared_ptr<http_response> response);
		bool try_to_extract_body_using_connection_closed(std::shared_ptr<http_response> response);


		void async_read_all_remaining_data(const boost::system::error_code& error, std::size_t bytes_transferred, std::shared_ptr<http_response> response, async_get_callback& callback) noexcept;

		void async_read_bytes(std::shared_ptr<http_response> response, async_get_callback& callback, std::size_t bytes_remaining) noexcept;
		void async_try_to_extract_body(std::shared_ptr<http_response> response, async_get_callback& callback) noexcept;
		void async_try_to_extract_body_using_current_lenght(std::shared_ptr<http_response> response, async_get_callback& callback) noexcept;
		void async_read_fragmented_body(std::shared_ptr<http_response> response, async_get_callback& callback, bool should_read_size = true, uint16_t buffer_size = 0) noexcept;
		void async_try_to_extract_body_using_transfer_encoding(std::shared_ptr<http_response> response, async_get_callback& callback) noexcept;
		void async_try_to_extract_body_using_connection_closed(std::shared_ptr<http_response> response, async_get_callback& callback) noexcept;

	private:
		std::mutex m_mutex;
		std::atomic_bool m_waiting_for_response = false;
		std::shared_ptr<boost::asio::ip::tcp::socket> m_socket;
		boost::asio::streambuf m_request_buff;
	};
}
