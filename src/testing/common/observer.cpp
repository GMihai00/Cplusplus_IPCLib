#include "observer.hpp"

namespace utile
{
	observer::observer(const std::function<void()>& callback) : m_callback(callback) {}

	void observer::notify()
	{
		if (m_callback)
			m_callback();
	}

}
