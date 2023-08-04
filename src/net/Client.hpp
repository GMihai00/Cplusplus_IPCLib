#pragma once

#include <iostream>
#include <memory>
#include <thread>
#include <mutex>

#include <optional>
#include <condition_variable>
#include <chrono>
#include <shared_mutex>

#include <boost/asio.hpp>
#include <boost/asio/ts/buffer.hpp>
#include <boost/asio/ts/internet.hpp>

#include "message.hpp"
#include "../utile/thread_safe_queue.hpp"
#include "connection.hpp"

#include "../utile/data_types.hpp"


namespace net
{
    template<typename T>
    class client
    {
    private:
        ::utile::thread_safe_queue<owned_message<T>> m_answears_recieved;
        boost::asio::io_context m_context;
        std::thread m_thread_context;
        std::mutex m_mutex_get;
        std::shared_mutex m_mutex_connection;
        std::condition_variable m_cond_var_get;
        std::unique_ptr<connection<T>> m_connection;
        std::atomic<bool> m_shutting_down = false;
        boost::asio::io_context::work m_idle_work;
    private:

        bool is_connected()
        {
            std::shared_lock lock(m_mutex_connection);

            return m_connection && m_connection->is_connected();
        }
    public:
        client() : m_idle_work(m_context)
        {
            m_thread_context = std::thread([this]() { m_context.run(); });
        }

        virtual ~client() noexcept
        {
            m_shutting_down = true;
            disconnect();

            m_context.stop();

            m_cond_var_get.notify_one();
            if (m_thread_context.joinable())
                m_thread_context.join();
        }
    
        bool connect(const utile::IP_ADRESS& host, const utile::PORT port)
        {
            std::unique_lock lock(m_mutex_connection);

            try
            {
                boost::asio::ip::tcp::resolver resolver(m_context);
                boost::asio::ip::tcp::resolver::results_type endpoints =
                    resolver.resolve(host, std::to_string(port));

                m_connection = std::make_unique<connection<T>>(
                    owner::client,
                    m_context,
                    boost::asio::ip::tcp::socket(m_context),
                    m_answears_recieved,
                    m_cond_var_get);
            
                return m_connection->connect_to_server(endpoints);

            }
            catch(const std::exception& e)
            {
                // LOG_ERR << "client exception: " << e.what() << '\n';
                return false;
            }
        }
    
        void disconnect()
        {
            if (is_connected())
            {
                m_connection->disconnect();

                m_answears_recieved.clear();
            }
            m_connection.reset();
        }
    
        void send(const message<T>& msg)
	    {
		    if (is_connected())
			    m_connection->send(msg);
	    }

        std::optional<owned_message<T>> wait_for_answear(uint32_t timeout = 0)
        {
            if (timeout == 0)
            {
                std::unique_lock<std::mutex> ulock(m_mutex_get);
                if (m_answears_recieved.empty() && !m_shutting_down)
                    m_cond_var_get.wait(ulock, [&] { return !m_answears_recieved.empty() || m_shutting_down; });

                return m_answears_recieved.pop();
            }

            std::unique_lock<std::mutex> ulock(m_mutex_get);
            if (!m_answears_recieved.empty() || m_shutting_down)
            {
                return m_answears_recieved.pop();
            }

            if (m_cond_var_get.wait_for(ulock, std::chrono::milliseconds(timeout), [&] { return !m_answears_recieved.empty() || m_shutting_down; }))
            {
                return m_answears_recieved.pop();
            }
            else
            {
                // LOG_ERR << " Answear waiting timedout";
                return std::nullopt;
            }
        }
    };
} // namespace net

