#include "finally.hpp"
namespace utile
{
	finally::finally(const std::function<void()>& finall_action) : m_finall_action(finall_action)
	{
	}
	finally::~finally() noexcept
	{
		if (m_finall_action)
			m_finall_action();
	}
} // namespace utile
