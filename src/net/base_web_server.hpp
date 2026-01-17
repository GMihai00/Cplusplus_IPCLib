#pragma once

#include <regex>

#include <boost/asio.hpp>
#include <boost/asio/ts/buffer.hpp>
#include <boost/asio/ts/internet.hpp>
#include <boost/thread/thread.hpp>
#include <system_error>
#include <optional>
#include <functional>

#include "../utile/thread_safe_queue.hpp"
#include "../utile/thread_safe_map.hpp"

#include "http_request.hpp"
#include "web_message_controller.hpp"
#include "../utile/data_types.hpp"
#include "../utile/generic_error.hpp"

namespace net
{
	typedef std::function<http_response(std::shared_ptr<http_request>)> async_req_handle_callback;

	typedef std::function<http_response(std::shared_ptr<http_request>, const std::smatch& matches)> async_req_regex_handle_callback;

	template<typename T>
	class base_web_server
	{
	public:
		// can throw if invalid IP_ADRESS is present
		base_web_server(const utile::IP_ADRESS& host, const utile::PORT port, const uint64_t max_nr_connections, const uint64_t number_threads) : m_idle_work(m_context), m_connection_accepter(m_context)
		{
			assert(max_nr_connections > 0);
			assert(number_threads > 0);

			m_client_connection_handle = std::bind(&base_web_server::handle_client_connection, this, std::placeholders::_1);

			for (uint64_t it = 0; it < max_nr_connections; it++)
				m_available_connection_ids.push_unsafe(it);

			try
			{
				m_endpoint = boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string(host), port);
			}
			catch (...)
			{
				throw std::runtime_error("Invalid host name provided: " + host);
			}

			for (int i = 0; i < number_threads; i++)
			{
				m_worker_threads.create_thread(boost::bind(&base_web_server::worker_function, this));
			}
		}

		virtual ~base_web_server()
		{
			stop();

			m_worker_threads.join_all();
		}

		utile::web_error start()
		{
			{
				std::scoped_lock lock(m_mutex);

				if (m_connection_accepter.is_open())
				{
					return utile::web_error();
				}

				try
				{
					m_connection_accepter.open(m_endpoint.protocol());
					m_connection_accepter.bind(m_endpoint);
					m_connection_accepter.listen();
				}
				catch (const std::exception& err)
				{
					return utile::web_error(std::error_code(5, std::generic_category()), "Server exception: " + std::string(err.what()));
				}
			}

			wait_for_client_connection();

			return utile::web_error();
		}

		utile::web_error stop()
		{
			std::scoped_lock lock(m_mutex);

			if (!m_connection_accepter.is_open())
			{
				return utile::web_error();
			}

			m_connection_accepter.close();

			auto & locked = m_clients_controllers.lock();
			for (auto& [_, controller] : locked)
			{
				if (controller)
					disconnect(*controller);
			}
			m_clients_controllers.unlock();

			if (!m_context.stopped())
				m_context.stop();

			m_clients_controllers.clear();

			async_get_callback empty_get_callback = [](std::shared_ptr<ihttp_message>, utile::web_error) {};
			async_send_callback empty_send_callback = [](utile::web_error err) {};
			std::pair<async_get_callback, async_send_callback> empty_callback_pair(empty_get_callback, empty_send_callback);

			std::shared_ptr<std::pair<async_get_callback, async_send_callback> > empty_callback_ptr = std::make_shared<std::pair<async_get_callback, async_send_callback>>(empty_callback_pair);

			auto& locked_map = m_controllers_callbacks.lock();
			for (auto& [id, cbpair] : locked_map) cbpair = empty_callback_ptr;
			m_controllers_callbacks.unlock();

			return utile::web_error();
		}

		bool add_mapping(const request_type type, const std::string& method, async_req_handle_callback action)
		{
			return m_mappings[type].emplace(method, action).second;
		}

		void add_regex_mapping(const request_type type, const std::regex& pattern, async_req_regex_handle_callback action)
		{
			m_regex_mappings[type].push_back({ pattern, action });
		}

		void remove_mapping(const request_type type, const std::string& method)
		{
			m_mappings[type].erase(method);
		}
	protected:
		virtual bool can_client_connect(const std::shared_ptr<T> client) noexcept
		{
			return true;
		}
		virtual void on_client_connect(const std::shared_ptr<T> client) noexcept
		{

		}
		virtual void on_client_disconnect(const std::shared_ptr<T> client) noexcept
		{
#ifdef DEBUG
			std::cout << "Client with ip: \"" << client->lowest_layer().remote_endpoint().address().to_string() << "\" disconnected\n";
#endif
		}
		void set_build_client_socket_function(const std::function<std::shared_ptr<T>(boost::asio::io_context&)>& build_function) noexcept
		{
			m_build_client_socket_function = build_function;
		}

		void set_handshake_function(const std::function<void(std::shared_ptr<T>, std::function<void(std::shared_ptr<T>)>)>& handshake_function) noexcept
		{
			m_handshake_function = handshake_function;
		}
	private:

		void wait_for_client_connection() noexcept
		{
			std::scoped_lock lock(m_mutex);

			if (m_connection_accepter.is_open())
			{
				auto client_socket = m_build_client_socket_function(m_context);

				m_connection_accepter.async_accept(client_socket->lowest_layer(),
					[this, client_socket](const std::error_code& errcode)
					{
						if (!errcode)
						{
#ifdef DEBUG
							std::cout << "Connection attempt from " << client_socket->lowest_layer().remote_endpoint() << std::endl;
#endif
							if (m_handshake_function)
							{
								m_handshake_function(client_socket, m_client_connection_handle);
							}
							else
							{
								m_client_connection_handle(client_socket);
							}
						}
						else
						{
							std::cerr << "Connection error: " << errcode.message() << std::endl;
						}

						while (m_available_connection_ids.empty() && m_connection_accepter.is_open())
						{
							std::this_thread::sleep_for(std::chrono::milliseconds(5000));
						}


						wait_for_client_connection();
					});
			}
		}

		void handle_client_connection(std::shared_ptr<T> client_socket)
		{
			if (can_client_connect(client_socket))
			{
				auto id = m_available_connection_ids.pop();

				if (id == std::nullopt)
				{
					std::cerr << "Internal error";
					return;
				}

				auto client_id = id.value();

				async_get_callback get_callback = std::bind(&base_web_server::on_message_async, this, client_id, std::placeholders::_1, std::placeholders::_2);
				async_send_callback send_callback = [this, client_id](utile::web_error err) {
					if (!err)
					{
						auto& locked_map = m_clients_controllers.lock();
						if (auto it = locked_map.find(client_id); it != locked_map.end())
						{
							if (it->second)
								disconnect(*(it->second));
							locked_map.erase(it);
							m_available_connection_ids.push(client_id);
						}
						m_clients_controllers.unlock();
						return;
					}

					auto& locked_map = m_clients_controllers.lock();
					
					std::shared_ptr<web_message_controller<T>> controller = nullptr;
					
					if (auto it = locked_map.find(client_id); it != locked_map.end())
					{
						controller = it->second;
					}
					m_clients_controllers.unlock();
					
					
					std::shared_ptr<std::pair<async_get_callback, async_send_callback>> controller_callbacks = nullptr;
					
					if (controller != nullptr)
					{
						auto locked_map2 = m_controllers_callbacks.lock();
						if (auto it2 = locked_map2.find(client_id); it2 != locked_map2.end())
						{
							controller_callbacks = it2->second;
						}
						m_controllers_callbacks.unlock();
						
						if (controller_callbacks != nullptr)
						{
							controller->async_get_request(controller_callbacks->first);
						}
					
					}
					
				};

				std::shared_ptr<std::pair<async_get_callback, async_send_callback>> callback_pair = std::make_shared<std::pair<async_get_callback, async_send_callback>>(get_callback, send_callback);
				
				// to be seen when to remove this one, memory just stays there sadly for now until replaced 
				
				auto& locked_callbacks = m_controllers_callbacks.lock();
				locked_callbacks.emplace(client_id, callback_pair);
				m_controllers_callbacks.unlock();

				auto& locked = m_clients_controllers.lock();
				auto ret = locked.emplace(client_id, std::make_shared<web_message_controller<T>>(client_socket));
				m_clients_controllers.unlock();

				if (!ret.second)
				{
					std::cerr << "Internal error";
					return;
				}
				else
				{
					// start listening to messages
					ret.first->second->async_get_request((*m_controllers_callbacks.get_copy(client_id))->first);
					on_client_connect(client_socket);
				}
			}
			else
			{
#ifdef DEBUG
				std::cout << "Connection has been denied\n";
#endif
			}
		}

		void on_message_async(const uint64_t client_id, std::shared_ptr<net::ihttp_message> msg, utile::web_error err) noexcept
		{
			if (!err)
			{
#ifdef DEBUG
				std::cerr << err.message() << "\n";
#endif
				auto& locked = m_clients_controllers.lock();    
				if (auto it = locked.find(client_id); it != locked.end())
				{
					if (it->second) {
						signal_bad_request(*(it->second));
					}
					async_get_callback empty_get_callback = [](std::shared_ptr<ihttp_message>, utile::web_error) {};
					async_send_callback empty_send_callback = [](utile::web_error err) {};
					std::shared_ptr<std::pair<async_get_callback, async_send_callback>> empty_callback_pair = std::make_shared<std::pair<async_get_callback, async_send_callback>>(empty_get_callback, empty_send_callback);
					m_controllers_callbacks.insert_or_assign(client_id, empty_callback_pair);
					
					if (it->second) {
						disconnect(*(it->second));
					}
					locked.erase(it);
					m_available_connection_ids.push(client_id);
				}
				m_clients_controllers.unlock();
				return;
			}

			auto req = std::dynamic_pointer_cast<net::http_request>(msg);

			if (req == nullptr)
			{
				std::cerr << "Internal error";
				return;
			}

			auto method = req->get_method();
			auto type = req->get_type();
			
			
			std::shared_ptr<web_message_controller<T>> client_controller = nullptr;
			
			auto locked_map = m_clients_controllers.lock();
			if (auto it = locked_map.find(client_id); it != locked_map.end())
			{
				client_controller = it->second;
			}
			m_clients_controllers.unlock();
			
			std::shared_ptr<std::pair<async_get_callback, async_send_callback>> controller_callbacks = nullptr;
			
			auto locked_map2 = m_controllers_callbacks.lock();
			if (auto it2 = locked_map2.find(client_id); it2 != locked_map2.end())
			{
				controller_callbacks = it2->second;
			}
			m_controllers_callbacks.unlock();
			
			if (client_controller == nullptr || controller_callbacks == nullptr)
			{
				return;
			}
			
			std::smatch matches;

			if (auto handle = find_apropriate_handle(type, method); handle != std::nullopt)
			{
				auto reply = ((*handle)->second)(req);

				client_controller->reply_async(std::move(reply), controller_callbacks->second);
			}
			else if (auto reqex_handle = find_apropriate_regex_handle(type, method, matches); reqex_handle != std::nullopt)
			{
				auto reply = ((*reqex_handle)->second)(req, matches);

				client_controller->reply_async(std::move(reply), controller_callbacks->second);
			}
			else
			{
				signal_bad_request(*client_controller);
			}

		}


		void signal_bad_request(web_message_controller<T>& client_controller) noexcept
		{
			http_response response(400, "Bad Request");
			client_controller.reply(response);
		}

		void disconnect(web_message_controller<T>& client_controller) noexcept  try
		{
			auto socket = client_controller.get_connection_socket();

			on_client_disconnect(socket);

			if (socket->lowest_layer().is_open())
				socket->lowest_layer().close();
		}
		catch (...)
		{

		}

		void worker_function()
		{
			m_context.run();
		}

		std::optional<std::map<std::string, async_req_handle_callback>::iterator> find_apropriate_handle(const request_type type, const std::string& method)
		{
			auto& mapping = m_mappings[type];

			if (auto it = mapping.find(method); it != mapping.end())
				return it;

			return std::nullopt;
		}

		std::optional<std::vector<std::pair<std::regex, async_req_regex_handle_callback>>::iterator> find_apropriate_regex_handle(const request_type type, const std::string& method, std::smatch& matches)
		{
			auto& mapping = m_regex_mappings[type];
			// ex regex ^(\/test\/id=(\d+))$ for /text/id=2
			auto it = mapping.begin();
			for (; it != mapping.end(); it++)
			{
				if (std::regex_search(method, matches, it->first))
				{
					return it;
				}
			}

			return std::nullopt;
		}

		boost::asio::io_context m_context;
		boost::asio::io_context::work m_idle_work;
		boost::asio::ip::tcp::endpoint m_endpoint;
		boost::asio::ip::tcp::acceptor m_connection_accepter;
		std::mutex m_mutex;
		boost::thread_group m_worker_threads;
		utile::thread_safe_queue<uint64_t> m_available_connection_ids;
		std::map<request_type, std::map<std::string, async_req_handle_callback>> m_mappings;
		std::map<request_type, std::vector<std::pair<std::regex, async_req_regex_handle_callback>>> m_regex_mappings;
		utile::thread_safe_map<uint64_t, std::shared_ptr<web_message_controller<T>>> m_clients_controllers;
		utile::thread_safe_map<uint64_t, std::shared_ptr<std::pair<async_get_callback, async_send_callback>>> m_controllers_callbacks;
		std::function<void(std::shared_ptr<T>)> m_client_connection_handle;
		std::function<std::shared_ptr<T>(boost::asio::io_context&)> m_build_client_socket_function = nullptr;
		std::function<void(std::shared_ptr<T>, std::function<void(std::shared_ptr<T>)>)> m_handshake_function = nullptr;
	};
}
