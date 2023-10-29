#pragma once

#include <boost/asio.hpp>
#include <boost/asio/ts/buffer.hpp>
#include <boost/asio/ts/internet.hpp>

#include <system_error>

#include "../utile/thread_safe_queue.hpp"

#include "http_request.hpp"
#include "web_message_controller.hpp"
#include "../utile/data_types.hpp"
#include "../utile/generic_error.hpp"

namespace net
{
    typedef std::function<http_response(std::shared_ptr<http_request>)> async_req_handle_callback;

	class web_server
	{
	public:
		web_server(const utile::IP_ADRESS& host, utile::PORT port, const uint64_t max_nr_connections = 1000);
		~web_server();

		utile::web_error start();
        utile::web_error stop();
        bool add_mapping(const std::string& method, async_req_handle_callback action);
        void remove_mapping(const std::string& method);
	protected:
		virtual bool can_client_connect(const std::shared_ptr<boost::asio::ip::tcp::socket> client) noexcept;
		virtual void on_client_connect(const std::shared_ptr<boost::asio::ip::tcp::socket> client) noexcept;
		virtual void on_client_disconnect(const std::shared_ptr<boost::asio::ip::tcp::socket> client) noexcept;
	private:
		void wait_for_client_connection() noexcept;
		void handle_client_connection(const std::shared_ptr<boost::asio::ip::tcp::socket>& client_socket) noexcept;
        void on_message_async(const uint64_t client_id, std::shared_ptr<net::ihttp_message> msg, utile::web_error err) noexcept;
		
		void signal_bad_request(const std::shared_ptr<web_message_controller> client_controller) noexcept;
		void disconnect(const std::shared_ptr<web_message_controller> client_controller) noexcept;

		boost::asio::ip::tcp::endpoint m_endpoint;
		boost::asio::ip::tcp::acceptor m_connection_accepter;
		boost::asio::io_context m_context;
        std::mutex m_mutex;
		std::thread m_thread_context;
		boost::asio::io_context::work m_idle_work;
        utile::thread_safe_queue<uint64_t> m_available_connection_ids;
        std::map<std::string, async_req_handle_callback> m_mappings;
        std::map<uint64_t, std::shared_ptr<web_message_controller>> m_clients_controllers;
		std::map<uint64_t, std::pair<async_get_callback, async_send_callback>> m_controllers_callbacks;
	};
} 