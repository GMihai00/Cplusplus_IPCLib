#include <iostream>

#include <thread>
#include <chrono>
#include <memory>
//#include "security/rsa.hpp"

#include "net/client.hpp"
#include "net/server.hpp"
#include "net/message.hpp"

constexpr auto IP_SERVER = "127.0.0.1";
constexpr auto PORT_SERVER = 500;

void attach_clients(const uint32_t nr_clients)
{
    // TO DO
    // spawn clients using executable 
}

void stress_test()
{
    for (auto i = 1; i <= 10000; i *= 100)
    {
        attach_clients(i);
    }
}

void data_validity_test()
{
    // TO DO
}

void secure_connect_test()
{
    // TO DO
}

void costum_data_sending_test()
{
    // TO DO
}

void big_data_sending_test()
{
    // TO DO this r.n. should fail I think, no fragmenting implemented
}


int main()
{
    stress_test();

    return 0;
}
