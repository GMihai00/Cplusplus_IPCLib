#pragma once

#include <algorithm>
#include <cstdint>
#include <memory>
#include <vector>
#include <set>
#include <map>
#include <thread>
#include <system_error>
#include <condition_variable>
#include <optional>
#include <string>

#include <boost/asio.hpp>
#include <boost/asio/ts/buffer.hpp>
#include <boost/asio/ts/internet.hpp>

#include "utile/thread_safe_queue.hpp"
#include "message.hpp"
#include "connection.hpp"

#include "../utile/data_types.hpp"
#include "client_disconnect_observer.hpp"


namespace net
{

    template<typename T>
    class server
    {
    protected:
        ::utile::thread_safe_queue<owned_message<T>> m_recieved_messages_queue;
        // TO DO ADD DATA STRUCTURE TO STORE ASYNC REQUESTS
        boost::asio::ip::tcp::endpoint m_endpoint;
        std::optional<boost::asio::ip::tcp::acceptor> m_connection_accepter = std::nullopt;
        std::set<std::shared_ptr<connection<T>>> m_connections;
        boost::asio::io_context m_context;
        std::thread m_thread_context;
        std::thread m_thread_update;
        std::condition_variable m_cond_var_update;
        std::condition_variable m_cond_var_start;
        std::mutex m_mutex_start;
        std::mutex m_mutex_update;
        std::mutex m_mutex_send;
        std::mutex m_mutex_send_async;

        std::atomic<bool> m_shutting_down = false;

        std::unique_ptr<client_disconnect_observer<T>> m_observer_disconnect;
        std::function<void(std::shared_ptr<connection<T>>)> m_disconnect_callback;

    private:

        void update() noexcept
        {
            std::unique_lock<std::mutex> ulock(m_mutex_update);
            m_cond_var_update.wait(ulock, [&] { return (!m_recieved_messages_queue.empty() && !m_context.stopped()) || m_shutting_down; });

            if (m_shutting_down)
            {
                return;
            }

            // LOG_DBG << "UPDATING: Handling new message";
            auto maybeMsg = m_recieved_messages_queue.pop();

            if (maybeMsg.has_value())
            {
                auto msg = maybeMsg.value();

                if (msg.m_remote)
                {
                    // to think this over
                    /*if (msg.m_msg.m_header.disconnecting == false)
                    {*/
                        on_message(msg.m_remote, msg.m_msg);
                   /* }
                    else
                    {
                        message_client(msg.remote, msg.msg);

                        msg.remote->disconnect();
                    }*/
                }
            }
        }
        // ASYNC OK
        void wait_for_client_connection() noexcept
        {
            m_connection_accepter->async_accept(
                [this](std::error_code errcode, boost::asio::ip::tcp::socket socket)
                {
                    if (!errcode)
                    {
                        std::cout << "Attempting to connect to " << socket.remote_endpoint() << std::endl;
                        std::shared_ptr<connection<T>> newconnection =
                            std::make_shared<connection<T>>(
                                owner::Server,
                                m_context,
                                std::move(socket),
                                m_recieved_messages_queue,
                                m_cond_var_update,
                                m_observer_disconnect);
                        
                        if (can_client_connect(newconnection))
                        {
                            auto connection_id = newconnection->get_id();
                            m_connections.insert(std::shared_ptr<connection<T>>(std::move(newconnection)));

                            // could be refactored
                            for (const auto& connection : m_connections)
                            {
                                if (connection->get_id() == connection_id)
                                {
                                    connection->connect_to_client();
                                    on_client_connect(connection);
                                }
                            }
                        }
                        else
                        {
                            std::cout << "connection has been denied\n";
                        }
                    }
                    else
                    {
                        std::cerr << "Connection error: " << errcode.message() << std::endl;
                    }

                    wait_for_client_connection();
                });
        }

        void wait_for_start() noexcept
        {
            std::unique_lock<std::mutex> ulock(m_mutex_start);

            if (m_shutting_down)
                return;

            m_cond_var_start.wait(ulock, [this] { return m_context.stopped() || m_shutting_down; });

            if (m_shutting_down)
                return;

            m_context.restart();
            m_context.run();
        }

        void disconnect_callback(std::shared_ptr<connection<T>> connection) noexcept
        {
            if (connection)
            {
                on_client_disconnect(connection);
            }
        }

    public:

        server(const utile::IP_ADRESS& host, utile::PORT port) try
        {
            m_disconnect_callback = std::bind(&server::disconnect_callback, this, std::placeholders::_1);
            m_observer_disconnect = std::make_unique<client_disconnect_observer<T>>(m_disconnect_callback);

            m_endpoint = boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string(host), port);

            m_context.stop();

            m_thread_update = std::thread([&]() { while (!m_shutting_down) { update(); }});
            m_thread_context = std::thread([this]() { while (!m_shutting_down) { wait_for_start(); }});
        }
        catch (const std::exception& err)
        {
            throw std::runtime_error("Failed to create server err: " + std::string(err.what()));
        }

        virtual ~server() noexcept
        {
            m_shutting_down = true;

            stop();

            m_cond_var_start.notify_one();
            if (m_thread_context.joinable())
                m_thread_context.join();

            m_cond_var_update.notify_one();
            if (m_thread_update.joinable())
                m_thread_update.join();
        }

        void start()
        {
            if (!m_context.stopped())
            {
                // ISSUE HERE WITH STOPPED!!!
                std::cout << "Server already started\n";
                return;
            }

            m_cond_var_start.notify_one();

            try
            {
                if (!m_connection_accepter)
                {
                    m_connection_accepter = boost::asio::ip::tcp::acceptor(m_context);
                }

                m_connection_accepter->open(m_endpoint.protocol());
                m_connection_accepter->bind(m_endpoint);
                m_connection_accepter->listen();

                wait_for_client_connection();
            }
            catch(const std::exception& err)
            {
                throw std::runtime_error("server exception: " + std::string(err.what()));
            }
        }
    
        void stop() noexcept
        {
            if (m_connection_accepter)
            {
                m_connection_accepter->close();
            }

            if (!m_context.stopped())
                m_context.stop();

            // I think it's a good ideea to remove already existing connections on stop, to be tested
            m_connections.clear();
        }
    
    protected:

        void async_message_client(std::shared_ptr<connection<T>> client, const message<T>& msg)
        {
            std::scoped_lock lock(m_mutex_send_async);

            if (m_context.stopped())
            {
                throw std::runtime_error("Can not send message, server is stoped");
            }


            if (client && client->is_connected())
            {
                client->send_async(msg);
            }
            else if (client)
            {
                on_client_disconnect(client);

                m_connections.erase(client);

                client.reset();
            }
            else
            {
                // LOG_ERR << "Invalid client disconnect";
            }
        }

        void message_client(std::shared_ptr<connection<T>> client, const message<T>& msg)
        {
            std::scoped_lock lock(m_mutex_send);

            if (m_context.stopped())
            {
                throw std::runtime_error("Can not send message, server is stoped");
            }

            if (client && client->is_connected())
            {
                client->send(msg);
            }
            else if (client)
            {
                on_client_disconnect(client);

                m_connections.erase(client);

                client.reset();
            }
            else
            {
                // LOG_ERR << "Invalid client disconnect";
            }
        }

        virtual bool can_client_connect([[maybe_unused]] std::shared_ptr<connection<T>> client) noexcept
        {
            return true;
        }

        virtual void on_client_connect([[maybe_unused]]  std::shared_ptr<connection<T>> client) noexcept
        {

        }
    
        virtual void on_client_disconnect([[maybe_unused]]  std::shared_ptr<connection<T>> client) noexcept
        {
        }
    
        // task can be handle in a async matter to think about a way to make this happen maybe create another sepparate class
        // maybe instead I could have a send_async and on_async_message and have it call on_message and que up futures inside  
        // will have to add apart from queue a set of pending tasks or smth alike
        virtual void on_message_async([[maybe_unused]] std::shared_ptr<connection<T>> client, [[maybe_unused]] message<T>& msg) noexcept
        {
            // TO DO, TO BE DISCUSSED IF I CAN JUST HAVE IT RUN ON_MESSAGE INSTEAD
        }

        virtual void on_message([[maybe_unused]] std::shared_ptr<connection<T>> client, [[maybe_unused]] message<T>& msg) noexcept
        {
        }
    };
} // namespace net

