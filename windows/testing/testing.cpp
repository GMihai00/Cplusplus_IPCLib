#include <iostream>

#include <thread>
#include <chrono>
#include <memory>
//#include "security/rsa.hpp"

#include "net/client.hpp"
#include "net/server.hpp"
#include "net/message.hpp"
#include "utile/finally.hpp"

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

int general_test(const int nr_clients, const std::wstring& task, const int timeout)
{
    std::cout << __FUNCTION__ << " " << "nr_clients: " << nr_clients;
    std::wcout << L" task: " << task << std::endl;

    auto server_pid = start_server();

    if (server_pid == 0)
    {
        std::cerr << "Failed to start server!";
        return ERROR_SERVICE_NEVER_STARTED;
    }

    utile::finally close_server{ [&server_pid]() {
            g_process_manager.close_process(server_pid, 1000);
        } };

    HANDLE h_failure_event = CreateEventW(NULL, TRUE, FALSE, L"Global\\TestFailed");

    if (h_failure_event == NULL) {
        std::cout << "Failed to create failure event. Error code: " << GetLastError() << std::endl;
        return 1;
    }

    utile::finally close_handle{ [&h_failure_event]() {
            CloseHandle(h_failure_event);
        } };

    auto client_pids = attach_clients(nr_clients, task, timeout);

    if (client_pids.size() != nr_clients)
    {
        std::cerr << "Failed to start all clients";
        return ERROR_INTERNAL_ERROR;
    }

    utile::finally close_clients{ [&client_pids, &timeout]() {
        g_process_manager.close_processes(client_pids, timeout);
        } };

    if (WaitForSingleObject(h_failure_event, timeout) == WAIT_OBJECT_0)
    {
        std::cerr << "Error event caught\n";
        return 1;
    }

    return 0;
}

int stress_test()
{
    uint32_t nr_clients = 1;
    do
    {
        if (auto ret = general_test(nr_clients, L"test", 5000); ret != 0)
        {
            std::cout << "Stress test failed";
            return ret;
        }

        nr_clients *= 10;
    } while (nr_clients < 100);

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

int big_data_sending_test()
{
    if (auto ret = general_test(10, L"big_data", 10000); ret != 0)
    {
        std::cout << "Big data test failed";
        return ret;
    }
    return 0;
}


int main()
{
    int ret = 0;
    if (ret = stress_test(); ret)
    {
        return ret;
    }

    if (ret = big_data_sending_test(); ret)
    {
        return ret;
    }

    return 0;
}
