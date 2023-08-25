#pragma once

#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <map>

#include "observer.hpp"

namespace utile
{
	template <typename... Args>
	class timer
	{
	protected:
		std::atomic<uint16_t> m_time_left;
		std::atomic<bool> m_expired = false;
		std::atomic<bool> m_paused = true;
		std::atomic<bool> m_should_stop = false;
		std::thread m_timer_thread;
		std::mutex m_mutex_resume;
		std::condition_variable m_cond_var_resume;
		std::map<std::shared_ptr<observer<Args...>>, std::tuple<Args...>> m_attached_observers;
	public:
		timer() = delete;
		timer(const uint16_t& timeout) : m_time_left(timeout / 1000)
		{
			m_timer_thread = std::thread([this]() {
				while (!m_should_stop)
				{
					{
						std::unique_lock<std::mutex> ulock(m_mutex_resume);
						if (m_paused || !m_should_stop)
							m_cond_var_resume.wait(ulock, [this]() { return m_paused == false || m_should_stop; });
					}

					while (!m_should_stop && m_time_left > 0)
					{
						std::unique_lock<std::mutex> ulock(m_mutex_resume);
						// not the gratest ideea as thread could of been on hold for longer but for now will do
						std::this_thread::sleep_for(std::chrono::seconds(1));
						m_time_left--;

						if (m_paused)
						{
							m_cond_var_resume.wait(ulock, [this]() { return (m_paused == false); });
						}
					}

					std::scoped_lock ulock(m_mutex_resume);
					m_expired = true;

					if (!m_should_stop)
					{
						for (const auto& [observer, args] : m_attached_observers)
						{
							std::apply([observer](Args... args) { observer->notify(args...); }, args);
						}
					}
				}
			});
		}

		timer(const timer&) = delete;
		timer(const timer&&) = delete;

		virtual ~timer() noexcept
		{
			m_cond_var_resume.notify_one();
			if (m_timer_thread.joinable())
				m_timer_thread.join();
		}

		void resume()
		{
			if (m_paused)
			{
				m_paused = false;
				m_cond_var_resume.notify_one();
			}
		}

		void reset(const uint16_t timeout = 0)
		{
			if (timeout / 1000 != 0)
				m_time_left = timeout / 1000;

			m_expired = false;
		}

		void pause() {
			m_paused = true;
		}

		void subscribe(const std::shared_ptr<observer<Args...>> obs, Args... val)
		{
			m_attached_observers.emplace(obs, std::make_tuple(val...));
		}

		void unsubscribe(const std::shared_ptr<observer<Args...>> obs)
		{
			if (auto it = m_attached_observers.find(obs); it != m_attached_observers.end())
			{
				m_attached_observers.erase(it);
			}
		}

		void unsubscribe_all()
		{
			m_attached_observers.clear();
		}
	};

} // namespace utile

