#pragma once

#include <iostream>
#include <vector>
#include <cstdint>
#include <memory>
#include <type_traits>

#include "connection.hpp"

namespace ipc
{
    namespace net
    {
        namespace type_helpers
        {
            template <typename T>
            struct is_c_data_type {
                static constexpr bool value = std::is_arithmetic<T>::value || std::is_same<T, char>::value ||
                    std::is_same<T, wchar_t>::value || std::is_same<T, char16_t>::value ||
                    std::is_same<T, char32_t>::value;
            };
        }

        template <typename T>
        struct message_header
        {
        private:
            static uint16_t ID;
            uint16_t id;
        public:
            T type{};
            size_t size{};

            message_header() : id{++ID} {
                if (!std::is_enum<T>::value)
                {
                    throw(std::runtime_error("Invalid data type provided, the type is suppose to be an enum"));
                }
            }
        };
    
        template <typename T>
        struct message
        {
            message_header<T> header{};
            std::vector<uint8_t> body{};

            size_t size() const
            {
                return body.size();
            }
        
            friend std::ostream& operator << (std::ostream& os, const message<T>& msg)
            {
                os  << "ID: " << msg.header.id 
                    << " Size: " << msg.header.size
                    << " Type: " << int(msg.header.type);
                return os;
            }
        
            template<typename E>
            friend message<T>& operator << (message<T>& msg, const E& data)
            {
                static_assert(type_helpers::is_c_data_type<E>::value && !std::is_pointer<E>::value, "Data can not be pushed");

                size_t size_before_push = msg.body.size();
                msg.body.resize(size_before_push + sizeof(E));
                std::memcpy(msg.body.data() + size_before_push, &data, sizeof(E));
                msg.header.size = msg.size();
            
                return msg;
            }
        
            template<typename E>
            friend message<T>& operator >> (message<T>& msg, E& data)
            {
                static_assert(type_helpers::is_c_data_type<E>::value && !std::is_pointer<E>::value, "Data can not be poped");
             
                if (msg.body.size() >= sizeof(E))
                {
                    size_t size_after_pop = msg.body.size() - sizeof(E);
                    std::memcpy(&data, msg.body.data() + size_after_pop, sizeof(E));
                    msg.body.resize(size_after_pop);
                    msg.header.size = msg.size();
                }
                else
                {
                    throw std::runtime_error("Failed to read data, insufficient bytes");
                }

                return msg;
            }

            void clear()
            {
                body.clear();
                header.id = 0;
                header.hasPriority = false;
                header.size = this->size();
            }

            message<T> clone()
            {
                message<T> copy;
                copy.header = this->header;
                copy.body = this->body;

                return copy;
            }

            message<T> build_reply()
            {
                message<T> reply;
                reply.header = this->header;
                reply.body = {};

                return reply;
            }
        };
    
        template <typename T>
        uint16_t message_header<T>::ID = 0;

        template <typename T>
        class connection;

        template<typename T>
        struct owned_message
        {
            std::shared_ptr<connection<T>> remote;
            message<T> msg;
        
            friend std::ostream& operator << (std::ostream& os, const owned_message<T>& msg)
            {
                os << msg.msg();
                return os;
            }
            owned_message(const std::shared_ptr<connection<T>>& remotec, const message<T>& msgc) :
                remote{ remotec },
                msg {msgc}
            {

            }
        };
    } // namespace net
} // namespace ipc
