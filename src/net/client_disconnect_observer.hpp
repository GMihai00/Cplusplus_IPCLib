#pragma once

#include <functional>
#include <mutex>
#include "connection.hpp"


namespace net
{
	template<typename T>
	class connection;

	template<typename T>
	class client_disconnect_observer
	{
		const std::function<void(std::shared_ptr<connection<T>>)>& callback_;
	public:
		client_disconnect_observer() = delete;
		client_disconnect_observer(const std::function<void(std::shared_ptr<connection<T>>)>& callback)
			: callback_(callback) {}
		~client_disconnect_observer() = default;
		void notify(std::shared_ptr<connection<T>> client)
		{
			if (callback_)
				callback_(client);
		}
	};
} // namespace net
