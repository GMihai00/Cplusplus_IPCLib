#pragma once

#include "../MessageTypes.hpp"
#include <map>

namespace ipc
{
namespace utile
{
    template <typename T>
    class MessageIdProvider
    {
    private:
        std::map<T, uint16_t> messageToId;
    public:
        MessageIdProvider() = default;
        MessageIdProvider(MessageIdProvider&) = delete;
        ~MessageIdProvider() = default;
    
        uint16_t provideId(T type)
        {
            if (messageToId.find(type) == messageToId.end())
                messageToId.insert({ type, 0 });
            return messageToId[type]++;
        }
    };
} // namespace utile
} // namespace ipc
