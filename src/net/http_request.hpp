#pragma once
#include <string>
#include <nlohmann/json.hpp>
#include "web_request_helpers.hpp"

namespace net
{
	class http_request
	{
	public:
		http_request() = delete;

		http_request(const request_type type, 
			const std::string& method,
			const content_type cont_type, 
			const nlohmann::json& additional_header_data = nullptr, 
			const std::vector<uint8_t>& body_data = std::vector<uint8_t>());

		void set_host(const std::string& host);

		std::string get_method() const;
		std::string to_string() const;
	private:
		request_type m_type;
		std::string m_method;
		content_type m_content_type;
		std::string m_host{};
		// searching only the first level of the json, adding just string and ints
		// ex: Accept: text/*
		nlohmann::json m_additional_header_data = nullptr;
		std::vector<uint8_t> m_body_data;
	};
}