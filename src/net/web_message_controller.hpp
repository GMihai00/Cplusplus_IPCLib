#pragma once

#include "web_message_dispatcher.hpp"
#include "web_message_reciever.hpp"

#include "../utile/observer.hpp"

namespace net
{
	class web_message_controller
	{
	public:
		web_message_controller() = delete;
		web_message_controller(std::shared_ptr<boost::asio::ip::tcp::socket> socket);

		std::pair<std::shared_ptr<http_response>, utile::web_error> send(http_request&& request, const uint16_t timeout = 0) noexcept;
		void send_async(http_request&& request, async_get_callback& callback) noexcept;

		void attack_timeout_observer(const std::shared_ptr<utile::observer<>>& obs);
		// TO DO: void remove_observer();
		utile::web_error start_listening_for_incoming_req(async_req_callback& callback) noexcept;
		void stop_listening_for_incoming_req() noexcept;

		utile::web_error reply(http_response& response) noexcept;
		void reply_async(http_response& response, async_send_callback& callback) noexcept;
	private:
		void get_response_post_async_send(utile::web_error err, async_get_callback& callback);

		web_message_dispatcher m_dispatcher;
		web_message_reciever m_reciever;
		std::shared_ptr<boost::asio::ip::tcp::socket> m_socket;
		std::function<void()> m_base_timeout_callback;
		std::mutex m_mutex;
		std::atomic_bool m_can_send = true;
		async_send_callback m_write_callback;
		std::vector<std::shared_ptr<utile::observer<>>> m_timeout_observers;
	};
}