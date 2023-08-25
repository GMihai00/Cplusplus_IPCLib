#pragma once

#include <iostream>
#include <vector>
#include <cstdint>
#include <memory>
#include <type_traits>

#include "connection.hpp"

#include "../utile/serializable_data.hpp"

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
    public:
        uint16_t m_id;
        T m_type{};
        size_t m_size{};

        message_header() : m_id{++ID} {
            if (!std::is_enum<T>::value)
            {
                throw(std::runtime_error("Invalid data type provided, the type is suppose to be an enum"));
            }
        }
    };
    
    template <typename T>
    struct message
    {
        message_header<T> m_header{};
        std::vector<uint8_t> m_body{};

        size_t size() const
        {
            return m_body.size();
        }
        
        friend std::ostream& operator << (std::ostream& os, const message<T>& msg)
        {
            os  << "ID: " << msg.m_header.m_id 
                << " Size: " << msg.m_header.m_size
                << " Type: " << int(msg.m_header.m_type);
            return os;
        }
        
        template<typename E>
        friend message<T>& operator << (message<T>& msg, const E& data)
        {
            static_assert(type_helpers::is_c_data_type<E>::value && !std::is_pointer<E>::value, "Data can not be pushed");

            size_t size_before_push = msg.m_body.size();
            msg.m_body.resize(size_before_push + sizeof(E));
            std::memcpy(msg.m_body.data() + size_before_push, &data, sizeof(E));
            msg.m_header.m_size = msg.size();
            
            return msg;
        }
        
        template<typename E>
        friend message<T>& operator >> (message<T>& msg, E& data)
        {
            static_assert(type_helpers::is_c_data_type<E>::value && !std::is_pointer<E>::value, "Data can not be poped");
             
            if (msg.m_body.size() >= sizeof(E))
            {
                size_t size_after_pop = msg.m_body.size() - sizeof(E);
                std::memcpy(&data, msg.m_body.data() + size_after_pop, sizeof(E));
                msg.m_body.resize(size_after_pop);
                msg.m_header.m_size = msg.size();
            }
            else
            {
                throw std::runtime_error("Failed to read data, insufficient bytes");
            }

            return msg;
        }

        friend message<T>& operator >> (message<T>& msg, std::vector<uint8_t>& bytes)
        {
            if (msg.m_body.size() >= sizeof(uint8_t) * bytes.size())
            {
                size_t size_after_pop = msg.m_body.size() - sizeof(uint8_t) * bytes.size();
                std::memcpy(bytes.data(), msg.m_body.data() + size_after_pop, sizeof(uint8_t) * bytes.size());
                msg.m_body.resize(size_after_pop);
                msg.m_header.m_size = msg.size();
            }
            else
            {
                throw std::runtime_error("Failed to read data, insufficient bytes");
            }

            return msg;
        }

        friend message<T>& operator << (message<T>& msg, const serializable_data* data)
        {
            assert(data);

            size_t size_before_push = msg.m_body.size();
            msg.m_body.resize(size_before_push + data->size());
            std::memcpy(msg.m_body.data() + size_before_push, &(data->deserialize()), data->size());
            msg.m_header.m_size = msg.size();

            return msg;
        }

        friend message<T>& operator << (message<T>& msg, const std::vector<uint8_t>& bytes)
        {
            size_t sizeBeforePush = msg.m_body.size();
            msg.m_body.resize(sizeBeforePush + (sizeof(uint8_t) * bytes.size()));
            std::memcpy(
                msg.m_body.data() + sizeBeforePush,
                bytes.data(),
                (sizeof(uint8_t) * bytes.size()));
            msg.m_header.m_size = msg.size();

            return msg;
        }

        friend message<T>& operator >> (message<T>& msg, serializable_data* data)
        {
            assert(data);

            if (msg.m_body.size() >= data->size())
            {
                size_t size_after_pop = msg.m_body.size() - data->size();
                std::vector<uint8_t> intermidiate(data->size());
                std::memcpy(&intermidiate, msg.m_body.data() + size_after_pop, data.size());

                data->serialize(intermidiate);

                msg.m_body.resize(size_after_pop);
                msg.m_header.m_size = msg.size();
            }
            else
            {
                throw std::runtime_error("Failed to read data, insufficient bytes");
            }


            return msg;
        }

        void clear()
        {
            m_body.clear();
            m_header.m_id = 0;
            m_header.m_size = this->size();
        }

        message<T> clone() const
        {
            message<T> copy;
            copy.m_header = this->m_header;
            copy.m_body = this->m_body;

            return copy;
        }

        message<T> build_reply()
        {
            message<T> reply;
            reply.m_header = this->m_header;
            reply.m_header.m_size = 0;
            
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
        std::shared_ptr<connection<T>> m_remote;
        message<T> m_msg;
        
        friend std::ostream& operator << (std::ostream& os, const owned_message<T>& msg)
        {
            os << msg.m_msg();
            return os;
        }
        owned_message(const std::shared_ptr<connection<T>>& remote, const message<T>& msg) :
            m_remote{ remote },
            m_msg {msg}
        {

        }
    };
} // namespace net

