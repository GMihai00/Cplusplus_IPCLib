#pragma once

#include <functional>
#include <optional>

namespace utile
{
	template <typename... Args>
	class observer
	{
		static uint32_t OBJ_ID;

		uint32_t m_id;
		std::function<void(Args...)>& m_callback;
	public:
		observer() = delete;
		observer(std::function<void(Args...)>& callback) : m_id(++OBJ_ID), m_callback(callback) {}
		~observer() = default;
		void notify(Args...)
		{
			if (m_callback)
				m_callback(Args...);

		}

		bool operator<(const observer& other) const {
			return m_id < other.m_id;
		}
	};

	template <typename... Args>
	uint32_t observer<Args...>::OBJ_ID = 0;
} // namespace utile
