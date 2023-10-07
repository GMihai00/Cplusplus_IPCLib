#pragma once
#include <string>
#include <nlohmann/json.hpp>
#include "web_request_helpers.hpp"

namespace net
{
	class https_request
	{
	public:
		https_request() = delete;
		https_request(const request_type type);
		std::string to_string() const;
	};
}