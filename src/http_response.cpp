#include "http_response.hpp"

namespace net
{


	boost::asio::streambuf& http_response::get_buffer()
	{
		return m_buffer;
	}

	nlohmann::json http_response::get_header() const
	{
		return m_header_data;
	}

	std::vector<uint8_t> http_response::get_body_raw() const
	{
		return m_body_data;
	}

	nlohmann::json http_response::get_json_body() const
	{
		return nlohmann::json::parse(std::string(m_body_data.begin(), m_body_data.end()));
	}

	void http_response::build_header_from_data_recieved()
	{
		const char* data = boost::asio::buffer_cast<const char*>(m_buffer.data());
		std::size_t size = m_buffer.size();

		std::string raw_header_data(data, size);
		

		m_buffer.consume(m_buffer.size());
	}
	
	boost::asio::streambuf m_buffer;
	std::string m_version;
	uint16_t m_status;
	std::string m_reason;
	nlohmann::json m_header_data;
	std::vector<uint8_t> m_body_data;

}