#pragma once

#include <iostream>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <future>
#include <memory>

#include "http_request.hpp"
#include "http_response.hpp"
#include "../utile/timer.hpp"
#include "web_message_controller.hpp"
#include "../utile/data_types.hpp"

#ifdef __linux__
	#define ERROR_INVALID_ADDRESS EVP_R_INVALID_DIGEST
	#define ERROR_INTERNAL_ERROR ERR_R_INTERNAL_ERROR
#endif

namespace net
{
	template <typename T>
	class base_web_client
	{
	public:
		base_web_client() :
			m_idle_work(m_io_service),
			m_controller(m_socket)
		{
			m_thread_context = std::thread([this]() { m_io_service.run(); });
		}

		virtual ~base_web_client()
		{
			if (m_socket->lowest_layer().is_open())
			{
				disconnect();
			}

			m_io_service.stop();

			if (m_thread_context.joinable())
				m_thread_context.join();
		}

		bool connect(const std::string& url, const utile::PORT& port)
		{
			return connect(url, std::to_string(port));
		}

		virtual bool connect(const std::string& url, const std::optional<std::string>& port = std::nullopt) noexcept = 0;

		// to be called if you want to cancel async request
		void disconnect()
		{
			std::scoped_lock lock(m_mutex);
			if (m_socket->lowest_layer().is_open())
			{
				m_socket->lowest_layer().close();
			}
		}

		std::pair<std::shared_ptr<http_response>, utile::web_error> send(http_request&& request, const uint16_t timeout = 0, const bool should_follow_redirects = false)
		{
			request.set_host(m_host);

			auto rez = m_controller.send(std::move(request), timeout, should_follow_redirects);

			if (should_follow_redirects && rez.second.value() == 301) try
			{
				auto redirect_json_data = nlohmann::json::parse(rez.second.message());

				auto time_left = 0u;

				if (auto it = redirect_json_data.find("remaining_time"); it != redirect_json_data.end() && it->is_number())
				{
					time_left = it->template get<uint16_t>();
				}

				if (auto redirect_data = net::from_json(redirect_json_data); redirect_data != std::nullopt)
				{
					disconnect();

					if (connect(redirect_data->m_host, redirect_data->m_port))
					{
						request.set_host(m_host);
						request.set_method(redirect_data->m_method);

						return send(std::move(request), time_left, should_follow_redirects);
					}
					else
					{
						// in case of failed connection stop
						return { rez.first, utile::web_error(std::error_code(ERROR_INVALID_ADDRESS, std::generic_category()), "Bad redirect")};
					}
				}
			}
			catch (const std::exception& err)
			{
				return { rez.first, utile::web_error(std::error_code(ERROR_INTERNAL_ERROR, std::generic_category()), err.what()) };
			}

			return rez;
		}

		void send_async(http_request&& request, async_get_callback& callback, const uint16_t timeout = 0, const bool should_follow_redirects = false) noexcept
		{
			if (!m_controller.can_send())
			{
				if (callback)
					callback( nullptr, utile::web_error(std::error_code(5, std::generic_category()), "Request already ongoing") );
				return;
			}

			m_get_callback = [this, &callback, &request, should_follow_redirects](std::shared_ptr<ihttp_message> message, utile::web_error err) {
				if (should_follow_redirects && err.value() == 301) try
				{
					auto redirect_json_data = nlohmann::json::parse(err.message());

					auto time_left = 0u;

					if (auto it = redirect_json_data.find("remaining_time");  it != redirect_json_data.end() && it->is_number())
					{
						time_left = it->get<uint16_t>();
					}

					if (auto redirect_data = net::from_json(redirect_json_data); redirect_data != std::nullopt)
					{
						disconnect();

						if (connect(redirect_data->m_host, redirect_data->m_port))
						{
							request.set_host(m_host);
							request.set_method(redirect_data->m_method);

							send_async(std::move(request), callback, time_left, should_follow_redirects);
						}
						else
						{
							// in case of failed connection stop
							if (callback)
								callback(message, utile::web_error(std::error_code(ERROR_INVALID_ADDRESS, std::generic_category()), "Bad redirect"));
						}
					}
				}
				catch (const std::exception& err)
				{
					if (callback)
						callback(message, utile::web_error(std::error_code(ERROR_INTERNAL_ERROR, std::generic_category()), err.what()));
				}
			};

			request.set_host(m_host);

			m_controller.send_async(std::move(request), callback, timeout, should_follow_redirects);
		}
	protected:

		void set_socket(std::shared_ptr<T> socket)
		{
			m_socket = socket;
			m_controller.set_socket(m_socket);
		}

		boost::asio::io_service m_io_service;
		boost::asio::io_context::work m_idle_work;
		std::string m_host{};
		std::mutex m_mutex;
		std::thread m_thread_context;
		web_message_controller<T> m_controller;
		std::shared_ptr<T> m_socket = nullptr;
		async_get_callback m_get_callback;

	};
} // namespace net