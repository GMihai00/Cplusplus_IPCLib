#pragma once

#include <boost/asio/ssl.hpp>
#include "base_web_client.hpp"

namespace net
{
	class secure_web_client : public base_web_client<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>>
	{
	public:
		secure_web_client(const std::vector<std::string>& cert_files, const std::function<bool(bool, boost::asio::ssl::verify_context& ctx)>& verify_certificate_callback = nullptr);
		virtual ~secure_web_client() = default;

		bool connect(const std::string& url, const std::optional<std::string>& port = std::nullopt) noexcept override;
	private:
		boost::asio::ssl::context m_ssl_context;
		std::function<bool(bool, boost::asio::ssl::verify_context& ctx)> m_verify_certificate_callback;
	};

} // namespace net