#pragma once

#include <set>

#include "web_message_dispatcher.hpp"
#include "web_message_reciever.hpp"

#include "../utile/observer.hpp"
#include "../utile/timer.hpp"
#include "../utile/finally.hpp"

namespace net
{
	template <typename T>
	class web_message_controller
	{
	public:
		web_message_controller(std::shared_ptr<T>& socket)
			: m_socket(socket)
			, m_dispatcher(m_socket)
			, m_reciever(m_socket)
		{
			m_base_timeout_callback = [this]()
			{
				if (this && m_socket->lowest_layer().is_open())
				{
					m_socket->lowest_layer().close();
				}
			};

			m_timeout_observers.insert(std::make_shared<utile::observer<>>(m_base_timeout_callback));

		}

		std::pair<std::shared_ptr<http_response>, utile::web_error> send(http_request&& request, const uint16_t timeout = 0) noexcept
		{
			assert(m_socket);

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

		void send_async(http_request&& request, async_get_callback& callback) noexcept
		{
			assert(m_socket);

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

		void attach_timeout_observer(const std::shared_ptr<utile::observer<>>& obs)
		{
			m_timeout_observers.insert(obs);
		}

		void remove_observer(const std::shared_ptr<utile::observer<>>& obs)
		{
			m_timeout_observers.erase(obs);
		}

		std::pair<std::shared_ptr<http_request>, utile::web_error> get_request() noexcept
		{
			assert(m_socket);

			return m_reciever.get<http_request>();
		}

		void async_get_request(async_get_callback& callback) noexcept
		{
			assert(m_socket);

			return m_reciever.async_get<http_request>(callback);
		}

		utile::web_error reply(http_response& response) noexcept
		{
			assert(m_socket);

			return m_dispatcher.send<http_response>(response);
		}

		void reply_async(http_response&& response, async_send_callback& callback) noexcept
		{
			assert(m_socket);

			{
				std::scoped_lock lock(m_mutex);

				if (!m_can_send)
				{
					if (callback)
						callback(utile::web_error(std::error_code(5, std::generic_category()), "Request already ongoing"));
					return;
				}

				m_write_callback = [this, &callback](utile::web_error err) {

					if (this)
					{
						{
							std::scoped_lock lock(m_mutex);
							m_can_send = true;
						}

						if (callback)
							callback(err);
					}
				};
				m_can_send = false;
			}

			return m_dispatcher.send_async<http_response>(response, m_write_callback);
		}

		std::shared_ptr<T>& get_connection_socket() noexcept
		{
			return m_socket;
		}

		void set_socket(std::shared_ptr<T>& socket)
		{
			m_socket = socket;
			m_dispatcher.set_socket(m_socket);
			m_reciever.set_socket(m_socket);
		}

	private:
		void get_response_post_async_send(utile::web_error err, async_get_callback& callback)
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

		web_message_dispatcher<T> m_dispatcher;
		web_message_reciever<T> m_reciever;
		std::shared_ptr<T> m_socket = nullptr;
		std::function<void()> m_base_timeout_callback;
		std::mutex m_mutex;
		bool m_can_send = true;
		async_send_callback m_write_callback;
		std::set<std::shared_ptr<utile::observer<>>, utile::observer_shared_ptr_comparator<>> m_timeout_observers;
	};
}