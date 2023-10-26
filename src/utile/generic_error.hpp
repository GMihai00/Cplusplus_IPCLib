#pragma once

#include <system_error>
#include <string>

#define REQUEST_ALREADY_ONGOING (utile::web_error(std::error_code(5, std::generic_category()), "Request already ongoing"))
#define INTERNAL_ERROR (utile::web_error(std::error_code(1000, std::generic_category()), "Internal error, please contact the owner of the module"))

namespace utile
{
	template <typename T>
	class generic_error
	{
	public:
		generic_error() {}

		generic_error(const std::error_code& err, const T& msg) : m_error(err), m_message(msg)
		{
		}

		int value() const noexcept
		{
			return m_error.value();
		}

		std::error_category category() const noexcept
		{
			return m_error.category();
		}

		T message() const noexcept
		{
			return m_message;
		}

		explicit operator bool() const noexcept
		{
			return m_error.value() == 0;
		}

	private:
		std::error_code m_error = std::error_code();
		T m_message;
	};

	typedef utile::generic_error<std::string> web_error;
	typedef utile::generic_error<std::wstring> web_werror;
}