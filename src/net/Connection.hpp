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


namespace ipc
{
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
            const owner owner_;
            std::thread threadRead_;
            std::thread threadWrite_;
            std::mutex mutexRead_;
            std::mutex mutexWrite_;
            std::condition_variable condVarRead_;
            std::condition_variable condVarWrite_;
            std::condition_variable& condVarUpdate_;
            boost::asio::io_context& context_;
            boost::asio::ip::tcp::socket socket_;
            ::utile::thread_safe_queue<owned_message<T>>& incomingmessages_;
            ::utile::thread_safe_queue<message<T>> outgoingmessages_;
            message<T> incomingTemporarymessage_;
            std::atomic_bool isReading_;
            std::atomic_bool isWriting_;
            std::atomic_bool shuttingDown_ = false;
            std::string ipAdress_;
            uint32_t id_;
            std::unique_ptr<client_disconnect_observer<T>>& observer_;

        private:
  /*          struct compare_connections {
                bool operator() (const connection<T>& a,const connection<T>& b) const {
                    return a.getId() < b.getId();
                }
            };*/

            bool readData(std::vector<uint8_t>& vBuffer, size_t toRead)
            {
                size_t left = toRead;
                while (left && !shuttingDown_)
                {
                    if (shuttingDown_)
                        return false;
            
                    boost::system::error_code errcode;
                    size_t read = socket_.read_some(boost::asio::buffer(vBuffer.data() + (toRead - left), left), errcode);
            
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
                std::unique_ptr<client_disconnect_observer<T>>& observer = nullptr) :
                owner_{ owner },
                context_{ context },
                socket_{ std::move(socket) },
                incomingmessages_{ incomingmessages },
                condVarUpdate_{condVarUpdate},
                observer_{observer},
                isWriting_{false},
                isReading_{false},
                id_{ ++connection<T>::ID }
            {
                threadWrite_ = std::thread([this]() { writemessages(); });
                threadRead_ = std::thread([this]() { readmessages(); });
            }

            virtual ~connection() noexcept
            { 
                shuttingDown_ = true;

                condVarRead_.notify_one();
                if (threadRead_.joinable())
                    threadRead_.join();

                condVarWrite_.notify_one();
                if (threadWrite_.joinable())
                    threadWrite_.join();

                disconnect(); 
            }
    
            owner getowner() const
            {
                return owner_;
            }

            uint32_t getId() const
            {
                return id_;
            }
    
            std::string getIpAdress() const
            {
                return ipAdress_;
            }

            bool connectToServer(const boost::asio::ip::tcp::resolver::results_type& endpoints)
            {
                if (owner_ == owner::Client)
                {
                    // LOG_SET_NAME("connection-SERVER");
                    std::function<void(std::error_code errcode, boost::asio::ip::tcp::endpoint endpoint)> connectCallback;
                    if (isReading_)
                    {
                        connectCallback = [this](std::error_code errcode, boost::asio::ip::tcp::endpoint /*endpoint*/)
                        {
                            if (errcode)
                            {
                                // LOG_ERR << "FAILED TO CONNECT TO SERVER: " << errcode.message();
                                socket_.close();
                            }
                        };
                    }
                    else
                    {
                        connectCallback = [this](std::error_code errcode, boost::asio::ip::tcp::endpoint /*endpoint*/)
                        {
                            if (!errcode)
                            {
                                // LOG_DBG <<"Started reading messages:";
                                condVarRead_.notify_one();
                            }
                            else
                            {
                                // LOG_ERR << "FAILED TO CONNECT TO SERVER: " << errcode.message();
                                socket_.close();
                            }
                        };
                    }

                    std::error_code ec;
                    boost::asio::ip::tcp::endpoint endpoint;
                    try
                    {
                        endpoint = boost::asio::connect(socket_, endpoints);
                    }
                    catch (boost::system::system_error const& err)
                    {
                        ec = err.code();
                    }
                   
                    connectCallback(ec, endpoint);
                    if (socket_.is_open())
                    {
                        isReading_ = true;
                        ipAdress_ = socket_.remote_endpoint().address().to_string();
                        return true;
                    }

                    return false;
                }
                return false;
            }
    
            bool connectToClient()
            {
                if (owner_ == owner::Server)
                {
                    if (socket_.is_open())
                    {
                        std::function<void()> connectCallback;
                        if (!isReading_)
                        {
                            connectCallback = [this]()
                            {
                                // LOG_DBG <<"Started reading messages";
                                condVarRead_.notify_one();
                            };
                        }
                        else
                        {
                            connectCallback =[this]()
                            {
                            };
                        }
                        isReading_ = true;
                        connectCallback();
                        return true;
                    }
                    return false;
                }
                return false;
            }
    
            bool isConnected() const
            {
                return socket_.is_open();
            }
    
            std::shared_ptr<connection<T>> get_shared()
            {
                if (shuttingDown_)
                    return nullptr;

                return this->shared_from_this();
            }
            void readmessages()
            {
                if (!isReading_ && !shuttingDown_)
                {
                    std::unique_lock<std::mutex> ulock(mutexRead_);
                    condVarRead_.wait(ulock, [this]() { return isReading_ || shuttingDown_; });
                    ulock.unlock();
                }

                while (!shuttingDown_)
                {
                    std::scoped_lock lock(mutexRead_);
                    if (!readHeader()) { break; }
                    if (!readBody()) { break; }
                    addToIncomingmessageQueue();
                    condVarUpdate_.notify_one();
                }
                isReading_ = false;
            }

            bool readHeader()
            {
                std::vector<uint8_t> vBuffer(sizeof(message_header<T>));
        
                if (!readData(vBuffer, sizeof(message_header<T>))) { return false; }

                std::memcpy(&incomingTemporarymessage_.header, vBuffer.data(), sizeof(message_header<T>));
                // LOG_DBG << "Finished reading header for message: " << incomingTemporarymessage_;
                return true;
            }
    
            bool readBody()
            {
                std::vector<uint8_t> vBuffer(incomingTemporarymessage_.header.size * sizeof(uint8_t));
    
                if (!readData(vBuffer, sizeof(uint8_t) * incomingTemporarymessage_.header.size)) { return false; }

                incomingTemporarymessage_ << vBuffer;
                // LOG_DBG << "Finished reading message: " << incomingTemporarymessage_;
                return true;
            }
    
            void addToIncomingmessageQueue()
            {
                if (owner_ == owner::Server)
                {
                    const auto& pair = std::make_pair(
                        owned_message<T>{get_shared(), incomingTemporarymessage_},
                        incomingTemporarymessage_.header.hasPriority);
                    incomingmessages_.push(pair);
                }
                else
                {
                    incomingmessages_.push(
                        std::make_pair(
                            owned_message<T>{nullptr, incomingTemporarymessage_},
                            incomingTemporarymessage_.header.hasPriority
                        ));
                }
                incomingTemporarymessage_.clear();
                // LOG_DBG << "Added message to incoming queue";
            }

            void send(const message<T>& msg)
            {
                std::function<void()> postCallback;
                std::pair<message<T>, bool> pair = std::make_pair(msg, msg.header.hasPriority);
                outgoingmessages_.push(pair);
                // LOG_DBG <<"Adding message to outgoing queue: " << msg;
                if (isWriting_)
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
                        condVarWrite_.notify_one();
                    };
                }
                isWriting_ = true;

                if (isConnected())
                {
                    boost::asio::post(context_, postCallback);
                }
                else
                {
                    // LOG_WARN << "Failed to post message, client is disconnected";
                }
            }
    
            void writemessages()
            {
                while (!shuttingDown_)
                {
                    if (!isWriting_ && !shuttingDown_)
                    {
                        std::unique_lock<std::mutex> ulock(mutexWrite_);
                        condVarWrite_.wait(ulock, [this]() { return isWriting_ || shuttingDown_; });
                        ulock.unlock();
                    }

                    while (!outgoingmessages_.empty())
                    {
                        std::scoped_lock lock(mutexWrite_);

                        if (shuttingDown_)
                            break;

                        auto outgoingMsg = outgoingmessages_.pop();
                        if (!outgoingMsg)
                        {
                            // LOG_ERR << "Failed to get image from queue";
                            return;
                        } 
                        const auto& outgoingmessage = outgoingMsg.value().first;

                        // LOG_DBG << "Started writing message: " << outgoingmessage;
                        if (!writeHeader(outgoingmessage)) { outgoingmessages_.clear(); break; }

                        if (outgoingmessage.header.size > 0)
                        {
                            if (!writeBody(outgoingmessage)) { outgoingmessages_.clear(); break; }
                        }
                        else
                        {
                            // LOG_DBG << "Finished writing message ";
                        }
                    }
                    isWriting_ = false;
                }
            }
    
            bool writeHeader(const message<T>& outgoingmessage)
            {
                boost::system::error_code errcode;
                boost::asio::write(socket_, boost::asio::buffer(&outgoingmessage.header, sizeof(message_header<T>)), errcode);
        
                if (errcode)
                {
                    // LOG_ERR << "Failed to write message header: " << errcode.message();
                    disconnect();
                    return false;
                }
                // LOG_DBG << "Finished writing header";
                return true;
            }
    
            bool writeBody(const message<T>& outgoingmessage)
            {
                boost::system::error_code errcode;
                boost::asio::write(socket_, boost::asio::buffer(outgoingmessage.body.data(), sizeof(uint8_t) * outgoingmessage.size()), errcode);
        
                if (errcode)
                {
                    // LOG_ERR << "Failed to write message body: " << errcode.message();
                    disconnect();
                    return false;
                }
                // LOG_DBG << "Finished writing message";
                return true;
            }
    
            void disconnect()
            {
                if (isConnected())
                {
                    if (observer_)
                        observer_->notify(this->shared_from_this());

                    boost::asio::post(context_, [this]() { socket_.close(); });
                }
            }

            bool operator<(const connection<T>& other) const {
                return id_ < other.id_;
            }

            void get_id() const
            {
                return id_;
            }
        };

        template <typename T>
        uint32_t connection<T>::ID = 0;

    }   // namespace net
}   // namespace ipc