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

namespace net
{
	template <typename T>
	class base_web_client
	{
	public:
		base_web_client() :
			m_idle_work(m_io_service),
			m_resolver(m_io_service),
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

		virtual bool connect(const std::string& url, const std::optional<utile::PORT>& port = std::nullopt) noexcept
		{
			{
				std::scoped_lock lock(m_mutex);
				if (m_socket->lowest_layer().is_open())
				{
					return false;
				}
			}

			std::string string_port = "http";

			if (port != std::nullopt)
			{
				string_port = std::to_string(*port);
			}

			boost::asio::ip::tcp::resolver::query query(url, string_port);
			boost::asio::connect(m_socket->lowest_layer(), m_resolver.resolve(query));
			m_socket->lowest_layer().set_option(boost::asio::ip::tcp::no_delay(true));

			m_host = url;

			return true;
		}

		// to be called if you want to cancel async request
		void disconnect()
		{
			std::scoped_lock lock(m_mutex);
			if (m_socket->lowest_layer().is_open())
			{
				m_socket->lowest_layer().close();
			}
		}

		std::pair<std::shared_ptr<http_response>, utile::web_error> send(http_request&& request, const uint16_t timeout = 0)
		{
			request.set_host(m_host);

			return m_controller.send(std::move(request), timeout);
		}

		void send_async(http_request&& request, async_get_callback& callback) noexcept
		{
			request.set_host(m_host);

			m_controller.send_async(std::move(request), callback);
		}
	protected:

		void set_socket(std::shared_ptr<T> socket)
		{
			m_socket = socket;
			m_controller.set_socket(m_socket);
		}

		boost::asio::io_service m_io_service;
		boost::asio::io_context::work m_idle_work;
		boost::asio::ip::tcp::resolver m_resolver;
		std::string m_host{};
		std::mutex m_mutex;
		std::thread m_thread_context;
		web_message_controller<T> m_controller;
	private:
		std::shared_ptr<T> m_socket = nullptr;
	};
} // namespace net