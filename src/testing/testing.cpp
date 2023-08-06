#include <iostream>

#include <thread>
#include <chrono>
//#include "security/rsa.hpp"

#include "net/client.hpp"
#include "net/server.hpp"
#include "net/message.hpp"

enum class VehicleDetectionMessages : uint8_t
{
    ACK, // for connection, it contains coordinates
    NACK,
    VDB, // Vehicle Data Broadcast - latitude longitude, latitude longitude
    VCDR, // Vehicle Client Detection Result - number of vehicles detected
    REDIRECT, // FOR PROXY REDIRECTION
    PUBLIC_KEY_REQ, // TO GET THE PUBLIC KEY
    SECURE_CONNECT // FOR ESTABLISHING CONNECTION BETWEEN JMS AND TO
};

int main()
{
    //security::rsa_wrapper a;

    net::server<VehicleDetectionMessages> server("127.0.0.1", 500);

    net::client<VehicleDetectionMessages> client;

    client.connect("127.0.0.1", 500);

    while (true)
    {
        net::message<VehicleDetectionMessages> msg{};

        msg.m_header.m_type = VehicleDetectionMessages::ACK;

        msg << 5;

        client.send(msg);

        auto ans = client.wait_for_answear(500);

        if (ans == std::nullopt)
        {
            std::cout << "Failed to recieve message\n";
        }
    }

    return 0;
}
