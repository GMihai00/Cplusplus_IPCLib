#include "https_response.hpp"

namespace net
{
	boost::asio::streambuf& https_response::get_buffer()
	{
		return m_buffer;
	}

	std::string https_response::to_string() const
	{
		return std::string(boost::asio::buffers_begin(m_buffer.data()),
			boost::asio::buffers_begin(m_buffer.data()) + m_buffer.size());
	}

	nlohmann::json https_response::to_json() const noexcept try
	{
		// not really this but smth alike
		return nlohmann::json::parse(to_string());
	}
	catch (std::exception& err)
	{

		return nlohmann::json();
	}

}