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
			, m_cancel_timer(0)
		{
			m_base_timeout_callback = [this]()
			{
				if (this && m_socket->lowest_layer().is_open())
				{
					m_socket->lowest_layer().close();
				}
			};

			m_base_timeout_observer = std::make_shared<utile::observer<>>(m_base_timeout_callback);
			m_cancel_timer.subscribe(m_base_timeout_observer);
		}

		std::pair<std::shared_ptr<http_response>, utile::web_error> send(http_request&& request, const uint16_t timeout = 0, const bool should_follow_redirects = false) noexcept
		{
			assert(m_socket);

			if (timeout)
			{
				m_cancel_timer.reset(timeout);
				m_cancel_timer.resume();
			}

			auto pause_timer = [this]() { m_cancel_timer.pause(); };

			if (auto err = m_dispatcher.send(request); !err)
			{
				return { nullptr, err };
			}

			auto response = m_reciever.template get<http_response>();

			if (should_follow_redirects && response.second && response.first && response.first->get_status() == 301)
			{
				if (auto redirect_location = get_redirect_location(response.first); redirect_location != std::nullopt)
				{
					if (redirect_location.value().m_port != "" || redirect_location.value().m_host != "")
					{
						// redirection to another client is done by the client not by the controller
						auto helper_err_data = redirect_location->to_json();

						if (m_cancel_timer.is_running())
						{
							m_cancel_timer.pause();
							helper_err_data.emplace("remaining_time", m_cancel_timer.get_time_left());
						}

						return { response.first, utile::web_error(std::error_code(301, std::generic_category()), helper_err_data.dump()) };
					}

					request.set_method(redirect_location.value().m_method);

					return send(std::move(request), 0, should_follow_redirects);
				}
			}

			return response;
		}

		void send_async(http_request&& request, async_get_callback& callback, const uint16_t timeout = 0, const bool should_follow_redirects = false) noexcept
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
					std::ref(request),
					std::ref(callback),
					should_follow_redirects);

				m_can_send = false;
			}

			if (timeout)
			{
				m_cancel_timer.reset(timeout);
				m_cancel_timer.resume();
			}

			m_dispatcher.send_async(request, m_write_callback);
		}

		void attach_timeout_observer(const std::shared_ptr<utile::observer<>>& obs)
		{
			m_cancel_timer.subscribe(obs);
		}

		void remove_observer(const std::shared_ptr<utile::observer<>>& obs)
		{
			m_cancel_timer.unsubscribe(obs);
		}

		std::pair<std::shared_ptr<http_request>, utile::web_error> get_request() noexcept
		{
			assert(m_socket);

			return m_reciever.template get<http_request>();
		}

		void async_get_request(async_get_callback& callback) noexcept
		{
			assert(m_socket);

			return m_reciever.template async_get<http_request>(callback);
		}

		utile::web_error reply(http_response& response) noexcept
		{
			assert(m_socket);

			return m_dispatcher.template send<http_response>(response);
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

			return m_dispatcher.template send_async<http_response>(response, m_write_callback);
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

		bool can_send() const
		{
			return m_can_send;
		}

	private:
		void get_response_post_async_send(utile::web_error err, http_request& request, async_get_callback& callback, const bool should_follow_redirects)
		{
			if (!err)
			{
				m_cancel_timer.pause();

				{
					std::scoped_lock lock(m_mutex);
					m_can_send = true;
				}

				if (callback) callback(nullptr, err);
				return;
			}

			m_get_callback = [this, &callback, &request, should_follow_redirects](std::shared_ptr<ihttp_message> message, utile::web_error err) {

				auto response = std::dynamic_pointer_cast<http_response>(message);
				if (should_follow_redirects && err && response && response->get_status() == 301)
				{
					if (auto redirect_location = get_redirect_location(message); redirect_location != std::nullopt)
					{
						if (redirect_location.value().m_port != "" || redirect_location.value().m_host != "")
						{
							// redirection to another client is done by the client not by the controller
							m_cancel_timer.pause();

							{
								std::scoped_lock lock(m_mutex);
								m_can_send = true;
							}

							auto helper_err_data = redirect_location->to_json();

							if (m_cancel_timer.is_running())
							{
								m_cancel_timer.pause();
								helper_err_data.emplace("remaining_time", m_cancel_timer.get_time_left());
							}

							callback(message, utile::web_error(std::error_code(301, std::generic_category()), helper_err_data.dump()));
						}

						request.set_method(redirect_location.value().m_method);

						{ 
							std::scoped_lock lock(m_mutex); 
							m_can_send = true; 
						}

						send_async(std::move(request), callback, should_follow_redirects);
					}
				}

				m_cancel_timer.pause();

				{
					std::scoped_lock lock(m_mutex);
					m_can_send = true;
				}

				callback(message, err);
			};

			m_reciever.template async_get<http_response>(m_get_callback);
		}

		web_message_dispatcher<T> m_dispatcher;
		web_message_reciever<T> m_reciever;
		std::shared_ptr<T> m_socket = nullptr;
		std::function<void()> m_base_timeout_callback;
		std::mutex m_mutex;
		bool m_can_send = true;
		utile::timer<> m_cancel_timer;
		async_send_callback m_write_callback;
		async_get_callback m_get_callback;
		std::shared_ptr<utile::observer<>> m_base_timeout_observer;
	};
}