#include "web_message_controller.hpp"

#include "../utile/timer.hpp"
#include "../utile/finally.hpp"

namespace net
{
	std::shared_ptr<boost::asio::ip::tcp::socket> web_message_controller::get_connection_socket() const noexcept
	{
		return m_socket;
	}

	void web_message_controller::get_response_post_async_send(utile::web_error err, async_get_callback& callback)
	{
		if (!err)
		{
			{
				std::scoped_lock lock(m_mutex);
				m_can_send = true;
			}

			if (callback) callback(nullptr, err);
			return;
		}

		auto delete_callback_on_exit = utile::finally([this]() { if (this) { std::scoped_lock lock(m_mutex); m_can_send = true; } });
		
		m_reciever.async_get<http_response>(callback);
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

		m_timeout_observers.insert(std::make_shared<utile::observer<>>(m_base_timeout_callback));

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

		return m_reciever.get<http_response>();
	}

	std::pair<std::shared_ptr<http_request>, utile::web_error> web_message_controller::get_request() noexcept
	{
		return m_reciever.get<http_request>();
	}

	void web_message_controller::async_get_request(async_get_callback& callback) noexcept
	{
		return m_reciever.async_get<http_request>(callback);
	}

	void web_message_controller::send_async(http_request&& request, async_get_callback& callback) noexcept
	{
		{
			std::scoped_lock lock(m_mutex);

			if (!m_can_send)
			{
				if (callback)
					callback(nullptr, utile::web_error(std::error_code(5, std::generic_category()), "Request already ongoing"));
				return;
			}

			m_write_callback = std::bind(&web_message_controller::get_response_post_async_send,
				this,
				std::placeholders::_1,
				std::ref(callback));

			m_can_send = false;
		}

		m_dispatcher.send_async(request, m_write_callback);
	}

	void web_message_controller::attach_timeout_observer(const std::shared_ptr<utile::observer<>>& obs)
	{
		m_timeout_observers.insert(obs);
	}

	void web_message_controller::remove_observer(const std::shared_ptr<utile::observer<>>& obs)
	{
		m_timeout_observers.erase(obs);
	}

	utile::web_error web_message_controller::reply(http_response& response) noexcept
	{
		return m_dispatcher.send<http_response>(response);
	}

	void web_message_controller::reply_async(http_response&& response, async_send_callback& callback) noexcept
	{
		{
			std::scoped_lock lock(m_mutex);

			if (!m_can_send)
			{
				if (callback)
					callback(utile::web_error(std::error_code(5, std::generic_category()), "Request already ongoing"));
				return;
			}
			// problem here on client disconnect when finished writting message
			// I kind of need a "safe callback" that has scoped_lock on call and on set/reset
			// instead of deleting it for good from the callback map I just reset it
			// somehow a way to avoid locking when calling
			m_write_callback = [this, &callback](utile::web_error err) { 

				if (this)
				{
					{
						std::scoped_lock lock(m_mutex);
						m_can_send = true;
					}

					if (callback && m_socket->is_open())
						callback(err);
				} 
			};
			m_can_send = false;
		}

		return m_dispatcher.send_async<http_response>(response, m_write_callback);
	}
}