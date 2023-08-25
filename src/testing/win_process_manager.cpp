#include "win_process_manager.hpp"
#include <iostream>
#include <algorithm>

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
    }

    std::filesystem::path win_process_manager::get_working_directory()
    {
        wchar_t pBuf[MAX_PATH];
        size_t len = sizeof(pBuf);

        int bytes = GetModuleFileName(NULL, pBuf, len);
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

        std::wcout << L"[INFO] Attempting to run: " << command << L"\n";

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

        std::cout << "Created new process with pid: " << static_cast<int>(pi.dwProcessId) << std::endl;

        CloseHandle(pi.hThread);

        m_running_processes.emplace(pi.dwProcessId, pi.hProcess);
        return pi.dwProcessId;
    }

    void win_process_manager::close_all_processes()
    {
        std::vector<HANDLE> v;
        std::transform(m_running_processes.begin(), m_running_processes.end(), std::inserter(v, v.begin()),
            [](const std::pair<DWORD, HANDLE>& pair) {
                return pair.second;
            });

        WaitForMultipleObjects(m_running_processes.size(), v.data(), FALSE, 1000);
    }

    void win_process_manager::close_process(const DWORD pid, const int timeout)
    {
        HANDLE h;
        if (auto it = m_running_processes.find(pid); it == m_running_processes.end())
            return;
        else
            h = it->second;

        std::cout << "Attempting to close process with pid: " << static_cast<int>(pid);

        WaitForSingleObject(h, timeout);

        if (!TerminateProcess(h, 0)) {
            std::cerr << "Error terminating process: " << details::GetLastErrorAsString() << std::endl;
            return;
        }

        CloseHandle(h);

        m_running_processes.erase(pid);
    }

    void win_process_manager::close_processes(const std::vector<DWORD>& pids, const int timeout)
    {
        std::vector<HANDLE> v;

        std::for_each(pids.begin(), pids.end(), [this, &v](const DWORD& pid) {
                
            if (auto it = m_running_processes.find(pid); it != m_running_processes.end())
                v.push_back(it->second);
            });

        std::cout << "Attempting to close " << v.size() << " processes";

        WaitForMultipleObjects(v.size(), v.data(), FALSE, timeout);
    }

    win_process_manager::~win_process_manager()
    {
        close_all_processes();
    }
}