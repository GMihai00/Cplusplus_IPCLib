#pragma once

#include <string>

#include <nlohmann/json.hpp>
#include <boost/asio.hpp>

#include "web_request_helpers.hpp"
#include "ihttp_message.hpp"

namespace net
{
	class http_request : public ihttp_message
	{
	public:
		http_request() = default;

		http_request(const request_type type, 
			const std::string& method,
			const content_type cont_type, 
			const nlohmann::json& header_data = nullptr,
			const std::vector<uint8_t>& body_data = std::vector<uint8_t>());

		void set_host(const std::string& host);
		std::string get_method() const;
		request_type get_type() const;
		virtual std::string to_string() const override;
		virtual bool load_header_prefix(std::istringstream& iss) noexcept override;
	private:
		request_type m_type = request_type::OPTIONS;
		std::string m_method{};
		content_type m_content_type = content_type::any;
		std::string m_host{};
	};
}