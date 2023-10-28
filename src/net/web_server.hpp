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
	private:

		void wait_for_client_connection() noexcept;

		boost::asio::ip::tcp::endpoint m_endpoint;
		boost::asio::ip::tcp::acceptor m_connection_accepter;
		boost::asio::io_context m_context;
        std::mutex m_mutex;
        
        utile::thread_safe_queue<uint64_t> m_available_connection_ids;

        std::map<std::string, async_req_handle_callback> m_mappings;

        std::map<uint64_t, std::shared_ptr<web_message_controller>> m_clients_controllers;

        // boost::asio::ip::tcp::socket socket instead of connection

        virtual bool can_client_connect([[maybe_unused]] const std::shared_ptr<boost::asio::ip::tcp::socket> client) noexcept
        {
            return true;
        }

        virtual void on_client_connect([[maybe_unused]] const std::shared_ptr<boost::asio::ip::tcp::socket> client) noexcept
        {

        }

        virtual void on_client_disconnect([[maybe_unused]] const std::shared_ptr<boost::asio::ip::tcp::socket> client) noexcept
        {
        }

        void on_message(const uint64_t client_id, std::shared_ptr<net::http_request> req, utile::web_error err) noexcept
        {
            if (!err)
            {
                // for debug only
                std::cerr << err.message() << "\n";
                return;
            }

            auto method = req->get_method();

            if (auto it = m_mappings.find(method); it != m_mappings.end())
            {
                if (auto it2 = m_clients_controllers.find(client_id); it2 != m_clients_controllers.end())
                {
                    auto reply = (it->second)(req);

                    it2->second->reply(reply);
                }
            }
        }

        void on_message_async(const uint64_t client_id, std::shared_ptr<net::http_request>& req, utile::web_error err) noexcept
        {
            if (!err)
            {
                // for debug only
                std::cerr << err.message() << "\n";
                return;
            }

           auto method = req->get_method();

           if (auto it = m_mappings.find(method); it != m_mappings.end())
           {
               if (auto it2 = m_clients_controllers.find(client_id); it2 != m_clients_controllers.end())
               {
                   auto reply = (it->second)(req);

                   /*TO DO*/ auto m_my_callback = async_send_callback();
                   it2->second->reply_async(reply, m_my_callback);
               }
           }
        }

        bool add_mapping(const std::string& method, async_req_handle_callback action)
        {
            return m_mappings.emplace(method, action).second;
        }

        void remove_mapping(const std::string& method)
        {
            m_mappings.erase(method);
        }
	};
} 