#pragma once

#include <iostream>
#include <vector>
#include <cstdint>
#include <memory>
#include <type_traits>

namespace ipc
{
    namespace net
    {
        template <typename T>
        struct IsCDataType {
            static constexpr bool value = std::is_arithmetic<T>::value || std::is_same<T, char>::value ||
                std::is_same<T, wchar_t>::value || std::is_same<T, char16_t>::value ||
                std::is_same<T, char32_t>::value;
        };

        template <typename T>
        struct MessageHeader
        {
            T type{};
            uint16_t id{};
            size_t size{};

            MessageHeader() {
                if (!std::is_enum<T>::value)
                {
                    throw(std::runtime_error("Invalid data type provided, the type is suppose to be an enum"));
                }
            }
        };
    
        template <typename T>
        struct Message
        {
            MessageHeader<T> header{};
            std::vector<uint8_t> body{};

            size_t size() const
            {
                return body.size();
            }
        
            friend std::ostream& operator << (std::ostream& os, const Message<T>& msg)
            {
                os  << "ID: " << msg.header.id 
                    << " Size: " << msg.header.size
                    << " Type: " << int(msg.header.type);
                return os;
            }
        
            template<typename DataType>
            friend Message<T>& operator << (Message<T>& msg, const DataType& data)
            {
                static_assert(IsCDataType<DataType>::value && !std::is_pointer<DataType>::value, "Data can not be pushed");

                size_t size_before_push = msg.body.size();
                msg.body.resize(size_before_push + sizeof(DataType));
                std::memcpy(msg.body.data() + size_before_push, &data, sizeof(DataType));
                msg.header.size = msg.size();
            
                return msg;
            }
        
            template<typename DataType>
            friend Message<T>& operator >> (Message<T>& msg, DataType& data)
            {
                static_assert(IsCDataType<DataType>::value && !std::is_pointer<DataType>::value, "Data can not be poped");
             
                if (msg.body.size() >= sizeof(DataType))
                {
                    size_t size_after_pop = msg.body.size() - sizeof(DataType);
                    std::memcpy(&data, msg.body.data() + size_after_pop, sizeof(DataType));
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

            Message<T> clone()
            {
                Message<T> copy;
                copy.header = this->header;
                copy.body = this->body;

                return copy;
            }
        };
    
        template <typename T>
        class Connection;

        template<typename T>
        struct OwnedMessage
        {
            std::shared_ptr<Connection<T>> remote;
            Message<T> msg;
        
            friend std::ostream& operator << (std::ostream& os, const OwnedMessage<T>& msg)
            {
                os << msg.msg();
                return os;
            }
            OwnedMessage(const std::shared_ptr<Connection<T>>& remotec, const Message<T>& msgc) :
                remote{ remotec },
                msg {msgc}
            {

            }
        };
    } // namespace net
} // namespace ipc
