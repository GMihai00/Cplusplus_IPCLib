#include <iostream>

#include "security/rsa.hpp"

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
    security::rsa_wrapper a;

    net::server<VehicleDetectionMessages> b("", 5);

    net::client<VehicleDetectionMessages> c();
    std::cout << "Hello World!\n";
}
