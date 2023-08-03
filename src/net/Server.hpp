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

#include "utile/ThreadSafeQueue.hpp"
#include "Message.hpp"
#include "Connection.hpp"

#include "../utile/IPCDataTypes.hpp"
#include "../utile/IPAdressHelpers.hpp"
#include "..\ClientDisconnectObserver.hpp"

namespace ipc
{
    namespace net
    {

        template<typename T>
        class Server
        {
        protected:
            ::utile::ThreadSafeQueue<OwnedMessage<T>> m_incomingMessagesQueue;
            boost::asio::io_context m_context;
            std::thread m_thread_context;
            std::thread threadUpdate_;
            std::condition_variable m_cond_var_update;
            std::condition_variable m_cond_var_start;
            std::mutex m_mutex_start;
            std::mutex m_mutex_update;
            std::mutex m_mutex_send;
            std::optional<boost::asio::ip::tcp::acceptor> m_connection_accepter = std::nullopt;

            std::set<std::shared_ptr<Connection<T>>> m_connections;
            std::atomic<bool> m_shutting_down = false;
            boost::asio::ip::tcp::endpoint m_endpoint;

            std::unique_ptr<ipc::utile::IClientDisconnectObserver<T>> m_observer_disconnect;
            std::function<void(std::shared_ptr<ipc::net::IConnection<T>>)> m_disconnect_callback;

        private:

            void update() noexcept
            {
                std::unique_lock<std::mutex> ulock(m_mutex_update);

                if (m_incomingMessagesQueue.empty() && !m_context.stopped() && !m_shutting_down)
                    m_cond_var_update.wait(ulock, [&] { return (!m_incomingMessagesQueue.empty() && !m_context.stopped()) || m_shutting_down; });

                if (m_shutting_down)
                {
                    return;
                }

                // LOG_DBG << "UPDATING: Handling new message";
                auto maybeMsg = m_incomingMessagesQueue.pop();

                if (maybeMsg.has_value())
                {
                    auto msg = maybeMsg.value().first;

                    if (msg.remote)
                    {
                        if (msg.msg.header.disconnecting == false)
                        {
                            onMessage(msg.remote, msg.msg);
                        }
                        else
                        {
                            message_client(msg.remote, msg.msg);

                            msg.remote->disconnect();
                        }
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
                            // LOG_INF << "Attempting to connect to " << socket.remote_endpoint();
                            std::shared_ptr<Connection<T>> newConnection =
                                std::make_shared<Connection<T>>(
                                    Owner::Server,
                                    m_context,
                                    std::move(socket),
                                    m_incomingMessagesQueue,
                                    m_cond_var_update,
                                    m_observer_disconnect);

                            if (on_client_connect(newConnection))
                            {
                                auto connection_id = newConnection->get_id();
                                m_connections.insert(std::shared_ptr<Connection<T>>(std::move(newConnection)));

                                // could be refactored
                                for (const auto& connection : m_connections)
                                {
                                    if (connection->get_id() == connection_id)
                                    {
                                        connection->connectToClient();
                                    }
                                }
                            }
                            else
                            {
                                // LOG_WARN << "Connection has been denied";
                            }
                        }
                        else
                        {
                            // LOG_ERR << "Connection Error " << errcode.message();
                        }

                        wait_for_client_connection();
                    });
            }

            void disconnect_callback(std::shared_ptr<ipc::net::IConnection<T>> connection) noexcept
            {
                std::shared_ptr<ipc::net::Connection<T>> connectionPtr =
                    std::dynamic_pointer_cast<ipc::net::Connection<T>>(connection);
                if (connectionPtr)
                {
                    on_client_disconnect(connectionPtr);
                }
            }

        public:

            Server(const utile::IP_ADRESS& host, ipc::utile::PORT port) try
            {
                m_disconnect_callback = std::bind(&Server::disconnect_callback, this, std::placeholders::_1);
                m_observer_disconnect = std::make_unique<ipc::utile::ClientDisconnectObserver<T>>(m_disconnect_callback);

                boost::asio::ip::tcp::endpoint m_endpoint(boost::asio::ip::address::from_string(host), port);

                threadUpdate_ = std::thread([&]() { while (!m_shutting_down) { update(); }});
                m_thread_context = std::thread([this]() { while (!m_shutting_down) { wait_for_start(); }});
            }
            catch (const std::exception& err)
            {
                throw std::runtime_error("Failed to create server err: " + std::string(err.what()));
            }

            virtual ~Server() noexcept
            {
                m_shutting_down = true;

                stop();

                if (m_thread_context.joinable())
                    m_thread_context.join();

                if (threadUpdate_.joinable())
                    threadUpdate_.join();
            }

            void wait_for_start() noexcept
            {
                std::unique_lock<std::mutex> ulock(m_mutex_start);

                if (m_shutting_down)
                    return;

                m_cond_var_start.wait(ulock, [&] { return true; });

                m_context.run();
            }

            void start() noexcept
            {
                if (!m_context.stopped())
                {
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
                    throw std::runtime_error("Server exception: " + std::string(err.what()));
                }
            }
    
            void stop() noexcept
            {
                if (m_connection_accepter)
                {
                    m_connection_accepter->close();
                }

                m_context.stop();

                m_cond_var_update.notify_one();
            }
    
            void message_client(std::shared_ptr<Connection<T>> client, const Message<T>& msg) noexcept
            {
                std::scoped_lock lock(m_mutex_send);

                if (m_context.stopped())
                {
                    throw std::runtime_error("Can not send message, server is stoped");
                }

                if (client && client->isConnected())
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
    
        protected:
            virtual bool on_client_connect([[maybe_unused]]  std::shared_ptr<Connection<T>> client) noexcept
            {
                return false;
            }
    
            virtual void on_client_disconnect([[maybe_unused]]  std::shared_ptr<Connection<T>> client) noexcept
            {
            }
    
            virtual void on_message([[maybe_unused]] std::shared_ptr<Connection<T>> client, [[maybe_unused]] Message<T>& msg) noexcept
            {
            }
        };
    } // namespace net
} // namespace ipc
