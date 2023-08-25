#pragma once

#include <Windows.h>
#include <unordered_map>
#include <vector>
#include <string>
#include <sstream>
#include <filesystem>
#include <mutex>

namespace win_helpers
{
    struct command_line
    {
        std::wstring m_exe;
        std::vector<std::wstring> m_arguments;

        inline std::wstring to_wstring() const
        {
            std::wstringstream wss;
            wss << L"Executable: " << m_exe << L"\n";
            wss << L"Arguments: ";
            for (const auto& arg : m_arguments)
            {
                wss << arg << L" ";
            }
            return wss.str();
        }
    };

    class win_process_manager
    {
    public:
        win_process_manager() = default;
        win_process_manager(win_process_manager&) = delete;
        win_process_manager(win_process_manager&&) = delete;

        std::vector<DWORD> create_processes_from_same_directory(const std::vector<command_line>& cmds);
        DWORD create_process_from_same_directory(const command_line& cmd);
        void close_all_processes();
        void close_process(const DWORD pid, const DWORD timeout);
        void close_processes(const std::vector<DWORD>& pids, const DWORD timeout);
        ~win_process_manager();
    private:
        std::filesystem::path get_working_directory();

        std::mutex m_mutex_map;
        std::unordered_map<DWORD, HANDLE> m_running_processes;
    };
}