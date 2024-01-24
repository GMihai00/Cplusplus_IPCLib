

#include "web_server.hpp"

#include <thread>
#include <chrono>

namespace net
{

    web_server::web_server(const utile::IP_ADRESS& host, const utile::PORT port, 
        const uint64_t max_nr_connections, const uint64_t number_threads)
        : base_web_server(host, port, max_nr_connections, number_threads)
    {
        m_build_client_socket_function = [](boost::asio::io_context& context) {
            return std::make_shared<boost::asio::ip::tcp::socket>(context);
        };

        set_build_client_socket_function(m_build_client_socket_function);
    }

}
