#pragma once

#include <vector>

namespace net
{
        
    class serializable_data
    {   
        serializable_data() = delete;
        virtual ~serializable_data() noexcept = default;

        virtual void serialize(std::vector<uint8_t>& data) = 0;
        virtual std::vector<uint8_t> deserialize() const = 0;
        virtual size_t size() const = 0;
    };
} // namespace net
