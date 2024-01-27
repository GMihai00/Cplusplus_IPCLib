#pragma once

#include "base_web_client.hpp"
#include <boost/asio/ssl.hpp>

namespace net
{
	class secure_web_client : public base_web_client<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>>
	{
	public:
		secure_web_client(const std::vector<std::string>& pem_files = {}, const std::function<bool(bool, boost::asio::ssl::verify_context& ctx)>& verify_certificate_callback = nullptr);
		virtual ~secure_web_client() = default;
		
		void set_verify_certificate_callback(const std::function<bool(bool, boost::asio::ssl::verify_context& ctx)>& verify_certificate_callback);
		void load_pem_files(const std::vector<std::string>& pem_files);
		virtual bool connect(const std::string& url, const std::optional<std::string>& port = std::nullopt) noexcept override;
	private:
		boost::asio::ssl::context m_ssl_context;
		std::function<bool(bool, boost::asio::ssl::verify_context& ctx)> m_verify_certificate_callback;
		boost::asio::ip::tcp::resolver m_resolver;
	};

} // namespace net