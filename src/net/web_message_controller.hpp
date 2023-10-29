#pragma once

#include <set>

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

		void attach_timeout_observer(const std::shared_ptr<utile::observer<>>& obs);
		void remove_observer(const std::shared_ptr<utile::observer<>>& obs);

		std::pair<std::shared_ptr<http_request>, utile::web_error> get_request() noexcept;
		void async_get_request(async_get_callback& callback) noexcept;

		utile::web_error reply(http_response& response) noexcept;
		void reply_async(http_response&& response, async_send_callback& callback) noexcept;

		std::shared_ptr<boost::asio::ip::tcp::socket> get_connection_socket() const noexcept;
	private:
		void get_response_post_async_send(utile::web_error err, async_get_callback& callback);

		web_message_dispatcher m_dispatcher;
		web_message_reciever m_reciever;
		std::shared_ptr<boost::asio::ip::tcp::socket> m_socket;
		std::function<void()> m_base_timeout_callback;
		std::mutex m_mutex;
		std::atomic_bool m_can_send = true;
		async_send_callback m_write_callback;
		std::set<std::shared_ptr<utile::observer<>>, utile::observer_shared_ptr_comparator<>> m_timeout_observers;
	};
}