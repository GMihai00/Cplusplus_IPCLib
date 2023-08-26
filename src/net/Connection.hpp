#pragma once

#include <iostream>
#include <memory>
#include <vector>
#include <system_error>
#include <mutex>
#include <thread>
#include <condition_variable>

#include <boost/asio.hpp>
#include <boost/asio/ts/buffer.hpp>
#include <boost/asio/ts/internet.hpp>

#include "client_disconnect_observer.hpp"

#include "message.hpp"
#include "../utile/thread_safe_queue.hpp"
#include "../utile/data_types.hpp"

namespace net
{
    enum class owner
    {
        Server,
        Client
    };

    template <typename T>
    class client_disconnect_observer;

    template<typename T>
    struct owned_message;

    template<typename T>
    struct message;

    template<typename T>
    struct message_header;

    template <typename T>
    class connection : public std::enable_shared_from_this<connection<T>>
    {
        static uint32_t ID;
    protected:
        const owner m_owner;
        std::thread m_thread_read;
        std::thread m_thread_write;
        std::mutex m_mutex_read;
        std::mutex m_mutex_write;
        std::condition_variable m_cond_var_read;
        std::condition_variable m_cond_var_write;
        std::condition_variable& m_cond_var_update;
        boost::asio::io_context& m_context;
        boost::asio::ip::tcp::socket m_socket;

        // TO DO ADD DATA STRUCTURES TO STORE ASYNC REQUESTS/ANSWEARS
        ::utile::thread_safe_queue<owned_message<T>>& m_incomming_messages;
        ::utile::thread_safe_queue<message<T>> m_outgoing_messages;
        message<T> m_incoming_message;
        std::atomic_bool m_reading = false;
        std::atomic_bool m_writing = false;
        std::atomic_bool m_shutting_down = false;
        std::string m_ip_adress;
        uint32_t m_id;
        std::unique_ptr<client_disconnect_observer<T>>& m_observer;

    private:
        bool read_data(std::vector<uint8_t>& vBuffer, size_t toRead)
        {
            size_t left = toRead;
            while (left && !m_shutting_down)
            {
                if (m_shutting_down)
                    return false;
            
                boost::system::error_code errcode;
                size_t read = m_socket.read_some(boost::asio::buffer(vBuffer.data() + (toRead - left), left), errcode);
            
                if (errcode)
                {
                        
                    // LOG_ERR << "Error while reading data err: " << errcode.value() << errcode.message();
                    disconnect();
                    return false;
                }
                left -= read;
            }
            return true;
        }
    public:
        connection(owner owner,
            boost::asio::io_context& context,
            boost::asio::ip::tcp::socket socket,
            ::utile::thread_safe_queue<owned_message<T>>& incomingmessages,
            std::condition_variable& condVarUpdate,
            std::unique_ptr<client_disconnect_observer<T>>& observer) :
            m_owner{ owner },
            m_context{ context },
            m_socket{ std::move(socket) },
            m_incomming_messages{ incomingmessages },
            m_cond_var_update{condVarUpdate},
            m_observer{observer},
            m_id{ ++connection<T>::ID }
        {
            m_thread_write = std::thread([this]() { write_messages(); });
            m_thread_read = std::thread([this]() { read_messages(); });
        }

        virtual ~connection() noexcept
        { 
            m_shutting_down = true;

            m_cond_var_read.notify_one();
            if (m_thread_read.joinable())
                m_thread_read.join();

            m_cond_var_write.notify_one();
            if (m_thread_write.joinable())
                m_thread_write.join();

            disconnect(); 
        }
    
        // this is not working properly
        utile::IP_ADRESS get_ip_adress() const
        {
            return m_ip_adress;
        }

        bool connect_to_server(const boost::asio::ip::tcp::resolver::results_type& endpoints)
        {
            if (m_owner == owner::Client)
            {
                // LOG_SET_NAME("connection-SERVER");
                std::function<void(const std::error_code&)> connect_callback;
                if (m_reading)
                {
                    connect_callback = [this](const std::error_code& errcode)
                    {
                        if (errcode)
                        {
                            std::cerr << "FAILED TO CONNECT TO SERVER: " << errcode.message();
                            m_socket.close();
                        }
                    };
                }
                else
                {
                    connect_callback = [this](const std::error_code& errcode)
                    {
                        if (!errcode)
                        {
                            std::cout <<"Started reading messages\n";
                            m_cond_var_read.notify_one();
                        }
                        else
                        {
                            std::cerr << "FAILED TO CONNECT TO SERVER: " << errcode.message();
                            m_socket.close();
                        }
                    };
                }

                std::error_code ec;
                boost::asio::ip::tcp::endpoint endpoint;
                try
                {
                    endpoint = boost::asio::connect(m_socket, endpoints);
                }
                catch (boost::system::system_error const& err)
                {
                    ec = err.code();
                }
                   
                connect_callback(ec);

                if (m_socket.is_open())
                {
                    m_reading = true;
                    m_ip_adress = m_socket.remote_endpoint().address().to_string();
                    return true;
                }

                return false;
            }
            return false;
        }
    
        bool connect_to_client()
        {
            if (m_owner == owner::Server)
            {
                if (m_socket.is_open())
                {
                    std::function<void()> connect_callback;
                    if (!m_reading)
                    {
                        connect_callback = [this]()
                        {
                            // LOG_DBG <<"Started reading messages";
                            m_cond_var_read.notify_one();
                        };
                    }
                    else
                    {
                        connect_callback =[this]()
                        {
                        };
                    }
                    m_reading = true;
                    connect_callback();
                    return true;
                }
                return false;
            }
            return false;
        }
    
        bool is_connected() const noexcept
        {
            return m_socket.is_open();
        }
    
        std::shared_ptr<connection<T>> get_shared()
        {
            if (m_shutting_down)
                return nullptr;

            return this->shared_from_this();
        }

        void read_messages()
        {
            if (!m_reading && !m_shutting_down)
            {
                std::unique_lock<std::mutex> ulock(m_mutex_read);
                m_cond_var_read.wait(ulock, [this]() { return m_reading || m_shutting_down; });
                ulock.unlock();
            }

            while (!m_shutting_down)
            {
                std::scoped_lock lock(m_mutex_read);

                if (!read_header()) { 
                    break; 
                }

                if (!read_body()) { 
                    break;
                }

                queue_message();

                m_cond_var_update.notify_one();
            }
            m_reading = false;
        }

        bool read_header()
        {
            std::vector<uint8_t> vBuffer(sizeof(message_header<T>));
        
            if (!read_data(vBuffer, sizeof(message_header<T>))) { return false; }

            std::memcpy(&m_incoming_message.m_header, vBuffer.data(), sizeof(message_header<T>));
            // LOG_DBG << "Finished reading header for message: " << m_incoming_message;
            return true;
        }
    
        bool read_body()
        {
            std::vector<uint8_t> vBuffer(m_incoming_message.m_header.m_size * sizeof(uint8_t));
    
            if (!read_data(vBuffer, sizeof(uint8_t) * m_incoming_message.m_header.m_size)) { return false; }

            m_incoming_message << vBuffer;
            // LOG_DBG << "Finished reading message: " << m_incoming_message;
            return true;
        }
    
        void queue_message()
        {
            if (m_owner == owner::Server)
            {
                const auto& msg = owned_message<T>{get_shared(), m_incoming_message};
                m_incomming_messages.push(msg);
            }
            else
            {
                m_incomming_messages.push(owned_message<T>{nullptr, m_incoming_message});
            }

            m_incoming_message.clear();
            // LOG_DBG << "Added message to incoming queue";
        }

        void send(const message<T>& msg)  noexcept
        {
            std::function<void()> postCallback;
            const message<T>& msg_cpy = msg.clone();
            m_outgoing_messages.push(msg_cpy);
            // LOG_DBG <<"Adding message to outgoing queue: " << msg;
            if (m_writing)
            {
                postCallback = [this, msg]()
                {
                };
            }
            else
            {
                postCallback = [this, msg]()
                {
                    // LOG_DBG <<"Started writing messages";
                    m_cond_var_write.notify_one();
                };
            }
            m_writing = true;

            if (is_connected())
            {
                boost::asio::post(m_context, postCallback);
            }
            else
            {
                // LOG_WARN << "Failed to post message, client is dis_connected";
            }
        }

        void send_async(const message<T>& msg) noexcept
        {
           // TO DO
        }
    
        void write_messages() noexcept
        {
            while (!m_shutting_down)
            {
                if (!m_writing && !m_shutting_down)
                {
                    std::unique_lock<std::mutex> ulock(m_mutex_write);
                    m_cond_var_write.wait(ulock, [this]() { return m_writing || m_shutting_down; });
                    ulock.unlock();
                }

                while (!m_outgoing_messages.empty())
                {
                    std::scoped_lock lock(m_mutex_write);

                    if (m_shutting_down)
                        break;

                    auto outgoingMsg = m_outgoing_messages.pop();
                    if (!outgoingMsg)
                    {
                        // LOG_ERR << "Failed to get image from queue";
                        return;
                    } 
                    const auto& outgoing_message = outgoingMsg.value();

                    // LOG_DBG << "Started writing message: " << outgoing_message;
                    if (!write_header(outgoing_message)) { m_outgoing_messages.clear(); break; }

                    if (outgoing_message.m_header.m_size > 0)
                    {
                        if (!write_body(outgoing_message)) { m_outgoing_messages.clear(); break; }
                    }
                    else
                    {
                        // LOG_DBG << "Finished writing message ";
                    }
                }
                m_writing = false;
            }
        }
    
        bool write_header(const message<T>& outgoing_message)  noexcept
        {
            boost::system::error_code errcode;
            boost::asio::write(m_socket, boost::asio::buffer(&outgoing_message.m_header, sizeof(message_header<T>)), errcode);
        
            if (errcode)
            {
                // LOG_ERR << "Failed to write message header: " << errcode.message();
                disconnect();
                return false;
            }
            // LOG_DBG << "Finished writing header";
            return true;
        }
    
        bool write_body(const message<T>& outgoing_message) noexcept
        {
            boost::system::error_code errcode;
            boost::asio::write(m_socket, boost::asio::buffer(outgoing_message.m_body.data(), sizeof(uint8_t) * outgoing_message.size()), errcode);
        
            if (errcode)
            {
                // LOG_ERR << "Failed to write message body: " << errcode.message();
                disconnect();
                return false;
            }
            // LOG_DBG << "Finished writing message";
            return true;
        }
    
        void disconnect() noexcept 
        {
            if (is_connected())
            {
                if (m_observer)
                    m_observer->notify(this->shared_from_this());

                boost::asio::post(m_context, [this]() { m_socket.close(); });
            }
        }

        bool operator<(const connection<T>& other) const {
            return m_id < other.m_id;
        }

        uint32_t get_id() const noexcept
        {
            return m_id;
        }
    };

    template <typename T>
    uint32_t connection<T>::ID = 0;

}   // namespace net