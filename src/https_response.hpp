#pragma once

#include <string>
#include <nlohmann/json.hpp>
#include <boost/asio.hpp>

namespace net
{
	class https_response
	{
	public:
		std::string to_string() const;
		nlohmann::json to_json() const noexcept;
		boost::asio::streambuf& get_buffer();
	private:
		boost::asio::streambuf m_buffer;
	};
}