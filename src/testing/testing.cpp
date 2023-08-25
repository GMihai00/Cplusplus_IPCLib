#include <iostream>

#include <thread>
#include <chrono>
#include <memory>
//#include "security/rsa.hpp"

#include "net/client.hpp"
#include "net/server.hpp"
#include "net/message.hpp"

#include "win_process_manager.hpp"

constexpr auto IP_SERVER = L"127.0.0.1";
constexpr auto PORT_SERVER = 5000;

win_helpers::win_process_manager g_process_manager;

DWORD start_server()
{
    win_helpers::command_line cmd;

    cmd.m_exe = L"testing_server.exe";
    cmd.m_arguments.push_back(L"--srv_ip");
    cmd.m_arguments.push_back(IP_SERVER);
    cmd.m_arguments.push_back(L"--srv_port");
    cmd.m_arguments.push_back(std::to_wstring(PORT_SERVER));

    return g_process_manager.create_process_from_same_directory(cmd);
}

std::vector<DWORD> attach_clients(const uint32_t nr_clients, const std::wstring& task, const int timeout)
{
    win_helpers::command_line cmd;

    cmd.m_exe = L"testing_client.exe";
    cmd.m_arguments.push_back(L"--srv_ip");
    cmd.m_arguments.push_back(IP_SERVER);
    cmd.m_arguments.push_back(L"--srv_port");
    cmd.m_arguments.push_back(std::to_wstring(PORT_SERVER));
    cmd.m_arguments.push_back(L"--cmd");
    cmd.m_arguments.push_back(task);
    cmd.m_arguments.push_back(L"--timeout");
    cmd.m_arguments.push_back(std::to_wstring(timeout));

    std::vector<win_helpers::command_line> cmds;

    for (uint32_t it = 0; it < nr_clients; it++)
    {
        cmds.push_back(cmd);
    }

    return g_process_manager.create_processes_from_same_directory(cmds);
}

int stress_test(const int nr_clients, const std::wstring& task, const int timeout)
{
    std::cout << __FUNCTION__ << " " << nr_clients << std::endl;
    auto server_pid = start_server();

    if (server_pid == 0)
    {
        std::cerr << "Failed to start server!";
        return ERROR_SERVICE_NEVER_STARTED;
    }

    auto client_pids = attach_clients(nr_clients, task, timeout);

    if (client_pids.size() != nr_clients)
    {
        std::cerr << "Failed to start all clients";
        return ERROR_INTERNAL_ERROR;
    }
    
    //takes time for client to initialize
    // trebuie regandita asta cu sleep, in multe cazuri still not enough somehow
    Sleep(5000);
    g_process_manager.close_processes(client_pids, timeout);

    // I QUESS POT SA PUN EVENT DE WIN PE STOP DE CLIENT SI SA NUMAR CATE EVENTS AU FOST PRIMITE INAPOI PE UN THREAD SEPARAT

    //AICI AR TREBUI SA ADAUG METODA SA ASTEPT DUPA TOATE PROCESELE DUPA EXIT CODE SI SA VERIFIC CA A FOST CU SUCCES

    g_process_manager.close_process(server_pid, 1000);
    return 0;
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

    uint32_t nr_clients = 1;
    do
    {
        if (auto ret = stress_test(nr_clients, L"test", 5000); ret != 0)
        {
            std::cout << "Stress test failed";
            return ret;
        }

        nr_clients *= 10;
    } while (nr_clients < 100);

    return 0;
}
