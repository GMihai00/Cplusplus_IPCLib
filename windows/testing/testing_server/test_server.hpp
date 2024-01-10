#pragma once

#include "../common/message_types.hpp"

#include "net/server.hpp"

class test_server : public net::server<TestingMessage>
{
private:
    // net::server
    virtual bool can_client_connect(std::shared_ptr<net::connection<TestingMessage>> client) noexcept override;
    virtual void on_client_connect(std::shared_ptr<net::connection<TestingMessage>> client) noexcept override;
    virtual void on_client_disconnect(std::shared_ptr<net::connection<TestingMessage>> client) noexcept override;
    virtual void on_message(std::shared_ptr<net::connection<TestingMessage>> client, net::message<TestingMessage>& msg) noexcept override;

    void approve_request(std::shared_ptr<net::connection<TestingMessage>> client, net::message<TestingMessage>& msg);
    void reject_request(std::shared_ptr<net::connection<TestingMessage>> client, net::message<TestingMessage>& msg);
    void send_back_big_data(std::shared_ptr<net::connection<TestingMessage>> client, net::message<TestingMessage>& msg);
    void do_idle_work();
public:
	test_server(const utile::IP_ADRESS& host, utile::PORT port);
    virtual ~test_server() noexcept = default;
};