#pragma once

#include <mutex>
#include <thread>
#include <queue>
#include <optional>
#include <shared_mutex>

namespace utile
{
    template<typename T>
    class ThreadSafeQueue
    {
    private:
        std::shared_mutex m_mutexRead;
        std::mutex m_mutexWrite;
        std::queue<T> m_queue;
    public:
        ThreadSafeQueue() = default;
        ThreadSafeQueue(const ThreadSafeQueue&) = delete;
        virtual ~ThreadSafeQueue() { clear(); }

        std::optional<T> pop()
        {
            if (m_queue.empty())
            {
                return {};
            }

            std::unique_lock lock(m_mutexRead);
            auto t = std::move(m_queue.front());
            m_queue.pop();
            return t;
        }

        void push(const T& elem)
        {
            std::scoped_lock lock(m_mutexWrite);
            m_queue.push(std::move(elem));
        }

        bool empty()
        {
            std::shared_lock lock(m_mutexRead);
            return m_queue.empty();
        }

        size_t size()
        {
            std::shared_lock lock(m_mutexRead);
            return m_queue.size();
        }

        void clear()
        {
            std::unique_lock lock(m_mutexRead);
            while (!m_queue.empty())
                m_queue.pop();
        }
    };
} // namespace utile
