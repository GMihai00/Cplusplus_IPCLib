
#include "web_server.hpp"

#include <thread>
#include <chrono>

namespace net
{
    web_server::web_server(const utile::IP_ADRESS& host, utile::PORT port, const uint64_t max_nr_connections) : m_idle_work(m_context), m_connection_accepter(m_context)
    {
        assert(max_nr_connections > 0);

        for (uint64_t it = 0; it < max_nr_connections; it++)
            m_available_connection_ids.push_unsafe(it);

        // specific port for web servers PORT to be removed from here
        m_endpoint = boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string(host), port);

        m_thread_context = std::thread([this]() { m_context.run(); });
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

        m_context.stop();

        if (m_thread_context.joinable())
            m_thread_context.join();
    }

    void web_server::handle_client_connection(const std::shared_ptr<boost::asio::ip::tcp::socket>& client_socket) noexcept
    {
        if (can_client_connect(client_socket))
        {
            auto id = m_available_connection_ids.pop();

            if (id == std::nullopt)
            {
                std::cerr << "Internal error";
                return;
            }

            auto client_id = id.value();

            async_get_callback get_callback = std::bind(&web_server::on_message_async, this, client_id, std::placeholders::_1, std::placeholders::_2);
            async_send_callback send_callback = [this, client_id](utile::web_error err) {
                if (!err)
                {
                    if (auto it = m_clients_controllers.find(client_id); it != m_clients_controllers.end())
                    {
                        disconnect(it->second);
                        m_clients_controllers.erase(it);
                        m_available_connection_ids.push(client_id);
                    }
                    return;
                }

                if (auto it = m_clients_controllers.find(client_id); it != m_clients_controllers.end())
                {
                    if (auto it2 = m_controllers_callbacks.find(client_id); it2 != m_controllers_callbacks.end())
                    {
                        it->second->async_get_request(it2->second.first);
                    }
                }
            };

            std::pair<async_get_callback, async_send_callback> callback_pair(get_callback, send_callback);

            // to be seen when to remove this one, memory just stays there saddly for now until replaced 
            m_controllers_callbacks[client_id] = callback_pair;

            if (auto ret = m_clients_controllers.emplace(client_id, std::make_shared<web_message_controller>(client_socket)); !ret.second)
            {
                std::cerr << "Internal error";
                return;
            }
            else
            {
                // start listening to messages
                ret.first->second->async_get_request(m_controllers_callbacks[client_id].first);
                on_client_connect(client_socket);
            }
        }
        else
        {
            // debug only
            std::cout << "Connection has been denied\n";
        }
    }

    void web_server::wait_for_client_connection() noexcept
    {
        std::scoped_lock lock(m_mutex);

        if (m_connection_accepter.is_open())
        {
            m_connection_accepter.async_accept(
                [this](std::error_code errcode, boost::asio::ip::tcp::socket socket)
                {
                    if (!errcode)
                    {
                        // debug only
                        std::cout << "Connection attempt from " << socket.remote_endpoint() << std::endl;

                        auto client_socket = std::make_shared<boost::asio::ip::tcp::socket>(std::move(socket));

                        handle_client_connection(client_socket);
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

    bool web_server::can_client_connect([[maybe_unused]] const std::shared_ptr<boost::asio::ip::tcp::socket> client) noexcept
    {
        return true;
    }

    void web_server::on_client_connect([[maybe_unused]] const std::shared_ptr<boost::asio::ip::tcp::socket> client) noexcept
    {

    }

    void web_server::on_client_disconnect([[maybe_unused]] const std::shared_ptr<boost::asio::ip::tcp::socket> client) noexcept
    {
    }

    void web_server::disconnect(const std::shared_ptr<web_message_controller> client_controller) noexcept try
    {
        auto socket = client_controller->get_connection_socket();

        on_client_disconnect(socket);

        if (socket->is_open())
            socket->close();
    }
    catch (...)
    {

    }

    void web_server::signal_bad_request(const std::shared_ptr<web_message_controller> client_controller) noexcept
    {
        http_response response(400, "Bad Request");
        client_controller->reply(response);
    }

    void web_server::on_message_async(const uint64_t client_id, std::shared_ptr<net::ihttp_message> msg, utile::web_error err) noexcept
    {
        auto req = std::dynamic_pointer_cast<net::http_request>(msg);

        if (req == nullptr)
        {
            std::cerr << "Internal error";
            return;
        }

        if (!err)
        {
            // for debug only
            std::cerr << err.message() << "\n";
            if (auto it = m_clients_controllers.find(client_id); it != m_clients_controllers.end())
            {
                signal_bad_request(it->second);
                disconnect(it->second);
                m_clients_controllers.erase(it);
                m_available_connection_ids.push(client_id);
            }
            return;
        }

        auto method = req->get_method();

        if (auto it = m_mappings.find(method); it != m_mappings.end())
        {
            if (auto it2 = m_clients_controllers.find(client_id); it2 != m_clients_controllers.end())
            {
                auto reply = (it->second)(req);

                /*TO DO*/ auto m_my_callback = async_send_callback(); // if send failed disconnect, if success listen to next requests
                it2->second->reply_async(std::move(reply), m_my_callback);
            }
        }
    }

    bool web_server::add_mapping(const std::string& method, async_req_handle_callback action)
    {
        return m_mappings.emplace(method, action).second;
    }

    void web_server::remove_mapping(const std::string& method)
    {
        m_mappings.erase(method);
    }


}
