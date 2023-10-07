#pragma once

#include <string>
#include <nlohmann/json.hpp>
#include <boost/asio.hpp>

namespace net
{
	class http_response
	{
	public:
		std::string to_string() const;
		boost::asio::streambuf& get_buffer();
		nlohmann::json get_header();
		std::vector<uint8_t> get_body_raw();
		nlohmann::json get_json_body();

		void build_header_from_data_recieved();
	private:
		boost::asio::streambuf m_buffer;
		nlohmann::json m_header_data;
		std::vector<uint8_t> m_body_data;
	};
}