#pragma once

#include <string>
#include <nlohmann/json.hpp>
#include <boost/asio.hpp>

namespace net
{
	class http_response
	{
	public:
		boost::asio::streambuf& get_buffer();
		nlohmann::json get_header() const;
		std::vector<uint8_t> get_body_raw() const;
		nlohmann::json get_json_body() const;

		void build_header_from_data_recieved();
	private:
		boost::asio::streambuf m_buffer;
		std::string m_version;
		uint16_t m_status;
		std::string m_reason;
		nlohmann::json m_header_data;
		std::vector<uint8_t> m_body_data;
	};
}