#pragma once

#include "base_web_client.hpp"

namespace net
{
	class web_client : public base_web_client<boost::asio::ip::tcp::socket>
	{
	public:
		web_client();
		virtual ~web_client() = default;
		virtual bool connect(const std::string& url, const std::optional<std::string>& port = std::nullopt) noexcept override;

	private:
		boost::asio::ip::tcp::resolver m_resolver;
	};
}