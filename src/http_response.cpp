#include "http_response.hpp"

namespace net
{
	boost::asio::streambuf& http_response::get_buffer()
	{
		return m_buffer;
	}

	std::string http_response::to_string() const
	{
		return std::string(boost::asio::buffers_begin(m_buffer.data()),
			boost::asio::buffers_begin(m_buffer.data()) + m_buffer.size());
	}

}