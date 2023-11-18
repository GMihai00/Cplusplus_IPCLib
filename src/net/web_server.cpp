

#include "web_server.hpp"

#include <thread>
#include <chrono>

namespace net
{
    void web_server::worker_function()
    {
        m_context.run();
    }

    web_server::web_server(const utile::IP_ADRESS& host, const utile::PORT port, const uint64_t max_nr_connections, const uint64_t number_threads) : m_idle_work(m_context), m_connection_accepter(m_context)
    {
        assert(max_nr_connections > 0);
        assert(number_threads > 0);

        for (uint64_t it = 0; it < max_nr_connections; it++)
            m_available_connection_ids.push_unsafe(it);

        // 80 HTTP 443 HTTPS
        try
        {
            m_endpoint = boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string(host), port);
        }
        catch (...)
        {
            throw std::runtime_error("Invalid host name provided: " + host);
        }

        for (int i = 0; i < number_threads; i++)
        {
            m_worker_threads.create_thread(boost::bind(&web_server::worker_function, this));
        }

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

        for (auto& [_, controller] : m_clients_controllers)
        {
            disconnect(controller);
        }

        if (!m_context.stopped())
            m_context.stop();

        m_clients_controllers.clear();

        async_get_callback empty_get_callback = [](std::shared_ptr<ihttp_message>, utile::web_error) {};
        async_send_callback empty_send_callback = [](utile::web_error err) {};
        std::pair<async_get_callback, async_send_callback> empty_callback_pair(empty_get_callback, empty_send_callback);
        
        for (const auto& [id, _] : m_controllers_callbacks)
        {
            m_controllers_callbacks[id] = empty_callback_pair;
        }

        return utile::web_error();
    }

    web_server::~web_server()
    {
        stop();

        m_worker_threads.join_all();
    }

    void web_server::handle_client_connection(std::shared_ptr<boost::asio::ip::tcp::socket> client_socket) noexcept
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
                        it->second.async_get_request(it2->second.first);
                    }
                }
            };

            std::pair<async_get_callback, async_send_callback> callback_pair(get_callback, send_callback);

            // to be seen when to remove this one, memory just stays there saddly for now until replaced 
            m_controllers_callbacks[client_id] = callback_pair;

            if (auto ret = m_clients_controllers.emplace(client_id, client_socket); !ret.second)
            {
                std::cerr << "Internal error";
                return;
            }
            else
            {
                // start listening to messages
                ret.first->second.async_get_request(m_controllers_callbacks[client_id].first);
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
        std::cout << "Client with ip: \"" << client->remote_endpoint().address().to_string() << "\" disconnected\n";
    }

    void web_server::disconnect(web_message_controller<boost::asio::ip::tcp::socket>& client_controller) noexcept try
    {
        auto socket = client_controller.get_connection_socket();

        on_client_disconnect(socket);

        if (socket->is_open())
            socket->close();
    }
    catch (...)
    {

    }

    void web_server::signal_bad_request(web_message_controller<boost::asio::ip::tcp::socket>& client_controller) noexcept
    {
        http_response response(400, "Bad Request");
        client_controller.reply(response);
    }

    std::optional<std::map<std::string, async_req_handle_callback>::iterator> web_server::find_apropriate_handle(const request_type type, const std::string& method)
    {
        auto& mapping = m_mappings[type];

        if (auto it = mapping.find(method); it != mapping.end())
            return it;

        return std::nullopt;
    }

    std::optional<std::vector<std::pair<std::regex, async_req_regex_handle_callback>>::iterator> web_server::find_apropriate_regex_handle(const request_type type, const std::string& method, std::smatch& matches)
    {
        auto& mapping = m_regex_mappings[type];
        // ex regex ^(\/test\/id=(\d+))$ for /text/id=2
        auto it = mapping.begin();
        for (; it != mapping.end(); it++)
        {
            if (std::regex_search(method, matches, it->first))
            {
                return it;
            }
        }

        return std::nullopt;
    }

    void web_server::on_message_async(const uint64_t client_id, std::shared_ptr<net::ihttp_message> msg, utile::web_error err) noexcept
    {
        if (!err)
        {
            // for debug only
            std::cerr << err.message() << "\n";

            if (auto it = m_clients_controllers.find(client_id); it != m_clients_controllers.end())
            {
                signal_bad_request(it->second);
                async_get_callback empty_get_callback = [](std::shared_ptr<ihttp_message>, utile::web_error) {};
                async_send_callback empty_send_callback = [](utile::web_error err) {};
                std::pair<async_get_callback, async_send_callback> empty_callback_pair(empty_get_callback, empty_send_callback);
                m_controllers_callbacks[client_id] = empty_callback_pair;
                disconnect(it->second);
                m_clients_controllers.erase(it);
                m_available_connection_ids.push(client_id);
            }
            return;
        }

        auto req = std::dynamic_pointer_cast<net::http_request>(msg);

        if (req == nullptr)
        {
            std::cerr << "Internal error";
            return;
        }

        auto method = req->get_method();
        auto type = req->get_type();

        if (auto it = m_clients_controllers.find(client_id); it != m_clients_controllers.end())
        {
            if (auto it2 = m_controllers_callbacks.find(client_id); it2 != m_controllers_callbacks.end())
            {
                std::smatch matches;

                if (auto handle = find_apropriate_handle(type, method); handle != std::nullopt)
                {
                    auto reply = ((*handle)->second)(req);

                    it->second.reply_async(std::move(reply), it2->second.second);
                }
                else if (auto reqex_handle = find_apropriate_regex_handle(type, method, matches); reqex_handle != std::nullopt)
                {
                    auto reply = ((*reqex_handle)->second)(req, matches);

                    it->second.reply_async(std::move(reply), it2->second.second);
                }
                else
                {
                    signal_bad_request(it->second);
                }
            }
        }
        
    }

    bool web_server::add_mapping(const request_type type, const std::string& method, async_req_handle_callback action)
    {
        return m_mappings[type].emplace(method, action).second;
    }

    void web_server::add_regex_mapping(const request_type type, const std::regex& pattern, async_req_regex_handle_callback action)
    {
        m_regex_mappings[type].push_back({ pattern, action });
    }

    void web_server::remove_mapping(const request_type type, const std::string& method)
    {
        m_mappings[type].erase(method);
    }


}
