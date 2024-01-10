#pragma once

#include <cinttypes>

enum class TestingMessage : uint8_t
{
    ACK, 
    NACK,
    TEST,
    NOK_MESSAGE,
    BIG_DATA
};