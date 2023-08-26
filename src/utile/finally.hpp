#pragma once

#include <functional>

namespace utile
{
	struct finally
	{
	private:
		std::function<void()> m_finall_action;
	public:
		finally() = delete;
		finally(const finally & obj) = delete;
		finally(const finally && obj) = delete;
		finally(const std::function<void()>& finall_action);
		~finally() noexcept;
	};

} // namespace utile
