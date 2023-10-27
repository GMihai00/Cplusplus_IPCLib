
#include "web_server.hpp"

#include <thread>
#include <chrono>

namespace net
{
    web_server::web_server(const utile::IP_ADRESS& host, utile::PORT port, const uint64_t max_nr_connections) : m_connection_accepter(m_context)
    {
        assert(max_nr_connections > 0);

        for (uint64_t it = 0; it < max_nr_connections; it++)
            m_available_connection_ids.push_unsafe(it);

        // specific port for web servers PORT to be removed from here
        m_endpoint = boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string(host), port);
    }

    utile::web_error web_server::start()
    {
        {
            std::scoped_lock lock(m_mutex);

            if (m_connection_accepter.is_open())
            {
                return utile::web_error();
            }

            try
            {
                m_connection_accepter.open(m_endpoint.protocol());
                m_connection_accepter.bind(m_endpoint);
                m_connection_accepter.listen();
            }
            catch (const std::exception& err)
            {
                return utile::web_error(std::error_code(5, std::generic_category()), "Server exception: " + std::string(err.what()));
            }
        }

        wait_for_client_connection();

        return utile::web_error();
    }

    utile::web_error web_server::stop()
    {
        std::scoped_lock lock(m_mutex);

        if (!m_connection_accepter.is_open())
        {
            return utile::web_error();
        }

        m_connection_accepter.close();

        return utile::web_error();
    }

    web_server::~web_server()
    {
        stop();
    }

    // ASYNC OK
    void web_server::wait_for_client_connection() noexcept
    {
        // to see what happends if I call async_accept when acceptor is not open
        m_connection_accepter.async_accept(
            [this](std::error_code errcode, boost::asio::ip::tcp::socket socket)
            {
                if (!errcode)
                {
                    // debug only
                    std::cout << "Connection attempt from " << socket.remote_endpoint() << std::endl;
                    
                    auto client_socket = std::make_shared<boost::asio::ip::tcp::socket>(std::move(socket));

                    if (can_client_connect(client_socket))
                    {
                        auto id = m_available_connection_ids.pop();

                        if (id == std::nullopt)
                        {
                            std::cerr << "Internal error";
                            return;
                        }

                        auto ret = m_clients_controllers.emplace(id.value(), std::make_shared<web_message_controller>(client_socket));
                        
                        if (!ret.second)
                        {
                            std::cerr << "Internal error";
                            return;
                        }
                        on_client_connect(client_socket);

                        // start listening to messages, method to be added to web_message_controller that waits for request
                    }
                    else
                    {
                        // debug only
                        std::cout << "Connection has been denied\n";
                    }
                }
                else
                {
                    std::cerr << "Connection error: " << errcode.message() << std::endl;
                }

                while (m_available_connection_ids.empty() && m_connection_accepter.is_open())
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(5000));
                }

                wait_for_client_connection();
            });
    }


}
