//#pragma once
//
//#include "base_web_client.hpp"
//
//namespace net
//{
//  // WIP send will not work, required to specify target of send
//	class udp_web_client : public base_web_client<boost::asio::ip::udp::socket>
//	{
//	public:
//		udp_web_client();
//		virtual ~udp_web_client() = default;
//		virtual bool connect(const std::string& url, const std::optional<std::string>& port = std::nullopt) noexcept override;
//
//	private:
//		boost::asio::ip::udp::resolver m_resolver;
//	};
//}