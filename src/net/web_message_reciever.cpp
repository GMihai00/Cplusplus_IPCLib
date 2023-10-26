#include "web_message_reciever.hpp"
/*
namespace net
{
	class web_message_reciever
	{
	public:
		web_message_reciever::web_message_reciever()
		{

		}

		utile::web_error web_message_reciever::get_response()
		{

		}

		utile::web_error web_message_reciever::async_get_response()
		{
		}

		void async_read(async_send_callback& callback) noexcept
		{

		}

		void try_to_extract_body(std::shared_ptr<http_response> response) noexcept
		{

		}
		bool try_to_extract_body_using_current_lenght(std::shared_ptr<http_response> response)
		{

		}
		bool try_to_extract_body_using_transfer_encoding(std::shared_ptr<http_response> response)
		{

		}
		bool try_to_extract_body_using_connection_closed(std::shared_ptr<http_response> response)
		{

		}

		void async_read_all_remaining_data(const boost::system::error_code& error, std::size_t bytes_transferred, std::shared_ptr<http_response> response, async_send_callback& callback) noexcept
		{
		}

		void async_read_bytes(std::shared_ptr<http_response> response, async_send_callback& callback, std::size_t bytes_remaining) noexcept
		{

		}

		void async_try_to_extract_body(std::shared_ptr<http_response> response, async_send_callback& callback) noexcept;
		void async_try_to_extract_body_using_current_lenght(std::shared_ptr<http_response> response, async_send_callback& callback) noexcept;
		void async_read_fragmented_body(std::shared_ptr<http_response> response, async_send_callback& callback, bool should_read_size = true, uint16_t buffer_size = 0) noexcept;
		void async_try_to_extract_body_using_transfer_encoding(std::shared_ptr<http_response> response, async_send_callback& callback) noexcept;
		void async_try_to_extract_body_using_connection_closed(std::shared_ptr<http_response> response, async_send_callback& callback) noexcept;

	private:
		std::mutex m_mutex;
		std::atomic_bool m_waiting_for_request = false;
		std::shared_ptr<boost::asio::ip::tcp::socket> m_socket;
	};
}*/