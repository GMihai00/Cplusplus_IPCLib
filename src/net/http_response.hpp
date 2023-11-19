#pragma once

#include <string>
#include <optional>

#include <nlohmann/json.hpp>
#include <boost/asio.hpp>

#include "ihttp_message.hpp"
namespace net
{
	class http_response : public ihttp_message
	{
	public:
		http_response() = default;

		http_response(const uint16_t status, const std::string reason, const nlohmann::json& header_data = nullptr,
			const std::vector<uint8_t>& body_data = std::vector<uint8_t>());
		std::string get_version() const;
		uint16_t get_status() const;
		std::string get_reason() const;

		virtual std::string to_string(const bool decrypt = false) const override;
		virtual bool load_header_prefix(std::istringstream& iss) noexcept override;

	private:
		std::string m_version{"HTTP/1.1"};
		uint16_t m_status{0};
		std::string m_reason{};
	};
}