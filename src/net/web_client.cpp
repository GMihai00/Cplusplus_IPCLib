#include "web_client.hpp"
#include "../utile/finally.hpp"
#include <boost/bind.hpp>

namespace net
{
	web_client::web_client() : base_web_client<boost::asio::ip::tcp::socket>()
	{
		set_socket(std::make_shared<boost::asio::ip::tcp::socket>(std::move(boost::asio::ip::tcp::socket(m_io_service))));
	}

} // namespace net