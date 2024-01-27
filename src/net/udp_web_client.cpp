//#include "udp_web_client.hpp"
//#include "../utile/finally.hpp"
//#include <boost/bind/bind.hpp>
//
//namespace net
//{
//	udp_web_client::udp_web_client() : base_web_client<boost::asio::ip::udp::socket>(), m_resolver(m_io_service)
//	{
//		set_socket(std::make_shared<boost::asio::ip::udp::socket>(std::move(boost::asio::ip::udp::socket(m_io_service))));
//
//		m_socket->open(boost::asio::ip::udp::v4());
//	}
//
//	bool udp_web_client::connect(const std::string& url, const std::optional<std::string>& port) noexcept
//	{
//		{
//			std::scoped_lock lock(m_mutex);
//			if (m_socket->lowest_layer().is_open())
//			{
//				return false;
//			}
//		}
//
//		if (port == std::nullopt)
//		{
//			return false;
//		}
//
//		std::string string_port = *port;
//
//		boost::asio::ip::udp::resolver::results_type endpoints = m_resolver.resolve(boost::asio::ip::udp::v4(), url, *port);
//
//		boost::asio::ip::udp::resolver::query query(url, string_port);
//		m_resolver.resolve(query);
//
//		
//		return false;
//	}
//} // namespace net
