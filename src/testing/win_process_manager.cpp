#include "win_process_manager.hpp"
#include <iostream>
#include <algorithm>
#include <execution>
#include <atomic>

#include "utile/finally.hpp"

namespace win_helpers
{
    namespace details
    {
        std::string GetLastErrorAsString()
        {
            DWORD errorMessageID = ::GetLastError();
            if (errorMessageID == 0) {
                return std::string();
            }

            LPSTR messageBuffer = nullptr;

            size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

            std::string message(messageBuffer, size);

            LocalFree(messageBuffer);

            return message;
        }
        std::wstring GenereateUniqueId()
        {
            GUID guid;
            if (CoCreateGuid(&guid) == S_OK) {
                wchar_t guid_str[39];
                if (StringFromGUID2(guid, guid_str, 39)) {
                    std::wcout << L"Generated GUID: " << guid_str << std::endl;
                    return std::wstring(guid_str);
                }
                else {
                    std::wcerr << L"String conversion failed." << std::endl;
                }
            }
            else {
                std::wcerr << L"Failed to create GUID." << std::endl;
            }

            return L"";
        }
    }

    std::filesystem::path win_process_manager::get_working_directory()
    {
        wchar_t pBuf[MAX_PATH];
        size_t len = sizeof(pBuf);

        int bytes = GetModuleFileName(NULL, pBuf, static_cast<DWORD>(len));
        if (!bytes)
        {
            std::cerr << "Failed to get path to current running process";
            exit(5);
        }

        return std::filesystem::path(pBuf).parent_path();
    }

    DWORD win_process_manager::create_process_from_same_directory(const command_line& cmd)
    {
        STARTUPINFO si;
        PROCESS_INFORMATION pi;

        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        ZeroMemory(&pi, sizeof(pi));


        auto workingDir = get_working_directory();
        workingDir /= cmd.m_exe;

        auto command = L"\"" + workingDir.wstring() + L"\"";
        for (const auto& arg : cmd.m_arguments)
        {
            command += L" \"" + arg + L"\"";
        }

        auto start_event_name = L"Global" + details::GenereateUniqueId();

        command += L" \"--start_event_guid\"";
        command += L" \"" + start_event_name + L"\"";

        std::wcout << L"[INFO] Attempting to run: " << command << L"\n";

        HANDLE h_start_event = CreateEventW(NULL, TRUE, FALSE, start_event_name.c_str());

        if (h_start_event == NULL) {
            std::cout << "Failed to create failure event. Error code: " << GetLastError() << std::endl;
            return 1;
        }

        utile::finally close_handle{ [&h_start_event]() {
                CloseHandle(h_start_event);
            } };


        std::atomic_bool process_started = false;

        if (!CreateProcess(NULL,
            LPWSTR(command.c_str()),
            NULL,
            NULL,
            FALSE,
            CREATE_NEW_CONSOLE,
            NULL,
            NULL,
            &si,
            &pi)
            )
        {

            std::cerr << "CreateProcess failed" << details::GetLastErrorAsString() << std::endl;
            return 0;
        }

        if (WaitForSingleObject(h_start_event, 10000) == WAIT_OBJECT_0)
        {
            std::cout << "Created new process with pid: " << static_cast<int>(pi.dwProcessId) << std::endl;

            CloseHandle(pi.hThread);

            std::lock_guard<std::mutex> lock(m_mutex_map);
            m_running_processes.emplace(pi.dwProcessId, pi.hProcess);

            return pi.dwProcessId;
        }
        else
        {
            std::cerr << "Process start timeout" << std::endl;
            return 0;
        }
    }

    std::vector<DWORD> win_process_manager::create_processes_from_same_directory(const std::vector<command_line>& cmds)
    {
        std::mutex mutex_vector;
        std::vector<DWORD> pids;
        std::atomic_bool ok = true;

        std::for_each(std::execution::par, cmds.begin(), cmds.end(), [this, &pids, &ok, &mutex_vector](const command_line& cmd) {
            if (ok)
            {
                auto pid = create_process_from_same_directory(cmd);
                if (pid == 0)
                {
                    ok = false;
                }
                else
                {
                    std::lock_guard<std::mutex> lock(mutex_vector);
                    pids.push_back(pid);
                }
            }
        });

        return pids;
    }

    void win_process_manager::close_all_processes()
    {
        std::vector<HANDLE> v;
        std::transform(m_running_processes.begin(), m_running_processes.end(), std::inserter(v, v.begin()),
            [](const std::pair<DWORD, HANDLE>& pair) {
                return pair.second;
            });

        WaitForMultipleObjects(m_running_processes.size(), v.data(), FALSE, (DWORD)1000);

        std::for_each(std::execution::par, v.begin(), v.end(), [](const HANDLE& h) {
            if (!TerminateProcess(h, 0)) {
                // std::cerr << "Error terminating process: " << details::GetLastErrorAsString() << std::endl;
                return;
            }
            CloseHandle(h);
        });
    }

    void win_process_manager::close_process(const DWORD pid, const DWORD timeout)
    {
        HANDLE h;
        if (auto it = m_running_processes.find(pid); it == m_running_processes.end())
            return;
        else
        {
            h = it->second;
            m_running_processes.erase(pid);
        }

        std::cout << "Attempting to close process with pid: " << static_cast<int>(pid) << std::endl;

        WaitForSingleObject(h, timeout);

        if (!TerminateProcess(h, 0)) {
            // std::cerr << "Error terminating process: " << details::GetLastErrorAsString() << std::endl;
            return;
        }

        CloseHandle(h);
    }

    void win_process_manager::close_processes(const std::vector<DWORD>& pids, const DWORD timeout)
    {
        std::vector<HANDLE> v;

        std::for_each(pids.begin(), pids.end(), [this, &v](const DWORD& pid) {
            if (auto it = m_running_processes.find(pid); it != m_running_processes.end())
            {
                v.push_back(it->second);
                std::lock_guard<std::mutex> lock(m_mutex_map);
                m_running_processes.erase(pid);
            }
        });

        std::cout << "Attempting to close " << v.size() << " processes" << std::endl;

        WaitForMultipleObjects(v.size(), v.data(), FALSE, timeout);

        std::for_each(std::execution::par, v.begin(), v.end(), [this](const HANDLE& h) {
            if (!TerminateProcess(h, 0)) {
                // std::cerr << "Error terminating process: " << details::GetLastErrorAsString() << std::endl;
                return;
            }

            CloseHandle(h);
        });
    }

    win_process_manager::~win_process_manager()
    {
        close_all_processes();
    }
}