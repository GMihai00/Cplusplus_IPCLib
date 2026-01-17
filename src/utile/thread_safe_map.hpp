#pragma once

#include <map>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <utility>

namespace utile
{
	template <typename K, typename V, typename Compare = std::less<K>>
	class thread_safe_map
	{
	private:
		mutable std::shared_mutex m_mutex;
		std::map<K, V, Compare> m_map;

	public:
		thread_safe_map() = default;
		thread_safe_map(const thread_safe_map&) = delete;
		thread_safe_map& operator=(const thread_safe_map&) = delete;
        
        
        std::map<K, V, Compare>& lock() 
        {
            m_mutex.lock();
            return m_map;
        }

        const std::map<K, V, Compare>& lock() const
        {
            m_mutex.lock();
            return m_map;
        }
        
        void unlock() const
        {
            m_mutex.unlock();
        }

		bool empty() const
		{
			std::shared_lock lock(m_mutex);
			return m_map.empty();
		}

		size_t size() const
		{
			std::shared_lock lock(m_mutex);
			return m_map.size();
		}

		void clear()
		{
			std::unique_lock lock(m_mutex);
			m_map.clear();
		}

		bool contains(const K& key) const
		{
			std::shared_lock lock(m_mutex);
			return m_map.find(key) != m_map.end();
		}

		std::optional<V> get_copy(const K& key) const
		{
			std::shared_lock lock(m_mutex);
			if (auto it = m_map.find(key); it != m_map.end())
			{
				return it->second;
			}
			return std::nullopt;
		}

		std::optional<V> erase_and_get(const K& key)
		{
			std::unique_lock lock(m_mutex);
			if (auto it = m_map.find(key); it != m_map.end())
			{
				V value = std::move(it->second);
				m_map.erase(it);
				return value;
			}
			return std::nullopt;
		}

		bool erase(const K& key)
		{
			std::unique_lock lock(m_mutex);
			return m_map.erase(key) != 0;
		}


		bool insert_or_assign(const K& key, V value)
		{
			std::unique_lock lock(m_mutex);
			m_map.insert_or_assign(key, std::move(value));
			return true;
		}
	};
}
