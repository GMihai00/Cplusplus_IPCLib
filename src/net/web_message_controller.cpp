#include "web_message_controller.hpp"

#include "../utile/timer.hpp"

namespace net
{
	void web_message_controller::get_response_post_async_send(utile::web_error err, async_get_callback& callback)
	{
		if (!err)
		{
			if (callback) callback(nullptr, err);
			m_can_send = true;
			return;
		}

		m_reciever.async_get_response(callback);
	}

	web_message_controller::web_message_controller(std::shared_ptr<boost::asio::ip::tcp::socket> socket)
		: m_socket(socket)
		, m_dispatcher(socket)
		, m_reciever(socket)
	{
		assert(socket);

		m_base_timeout_callback = [this]() 
		{ 
			if (this && m_socket->is_open())
			{
				m_socket->close();
			}
		};

		m_timeout_observers.push_back(std::make_shared<utile::observer<>>(m_base_timeout_callback));

	}

	std::pair<std::shared_ptr<http_response>, utile::web_error> web_message_controller::send(http_request&& request, const uint16_t timeout) noexcept
	{
		utile::timer<> cancel_timer(0);

		for (const auto& timeout_observer : m_timeout_observers)
			cancel_timer.subscribe(timeout_observer);

		if (timeout)
		{
			cancel_timer.reset(timeout);
			cancel_timer.resume();
		}

		if (auto err = m_dispatcher.send(request); !err)
		{
			return { nullptr, err };
		}

		return m_reciever.get_response();
	}

	// LIMITATION FOR NOW TO SEND ONLY ONE MESSAGE AT A TIME. TO BE CHANGED!!!

	void web_message_controller::send_async(http_request&& request, async_get_callback& callback) noexcept
	{
		write_callback = std::bind(&web_message_controller::get_response_post_async_send,
			this,
			std::placeholders::_1,
			callback);

		m_dispatcher.send_async(request, write_callback);
	}

	void web_message_controller::attack_timeout_observer(const std::shared_ptr<utile::observer<>>& obs)
	{
		m_timeout_observers.push_back(obs);
	}

}