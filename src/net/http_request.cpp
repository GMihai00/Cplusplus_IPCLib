#include "http_request.hpp"
#include <sstream>
namespace net
{

	http_request::http_request(const request_type type,
		const std::string& method,
		const content_type cont_type,
		const nlohmann::json& additional_header_data,
		const std::vector<uint8_t>& body_data)
		: m_type(type)
		, m_method(method)
		, m_content_type(cont_type)
		, m_additional_header_data(additional_header_data)
		, m_body_data(body_data)
	{
		
	}

	void http_request::set_host(const std::string& host)
	{
		m_host = host;
	}
	
	std::string http_request::to_string() const
	{
		std::stringstream ss;

		// request type
		ss << request_type_to_string(m_type) << " " << m_method << " " << "HTTP/1.1" << "\r\n";

		// header
		ss << "Host: " << m_host << "\r\n";
		ss << "Content-type: " << content_type_to_string(m_content_type) << "\r\n";
		ss << "Content-Length: " << m_body_data.size() << "\r\n";

		if (m_additional_header_data != nullptr)
		{
			for (const auto& item : m_additional_header_data.items())
			{
				std::string key = item.key();
				
				ss << key << ": ";
				if (item.value().is_number())
				{
					ss << item.value().get<long double>() << "\r\n";
					continue;
				}
				if (item.value().is_string())
				{
					ss << item.value().get<std::string>() << "\r\n";
					continue;
				}
				if (item.value().is_boolean())
				{
					ss << (int)(item.value().get<bool>()) << "\r\n";
					continue;
				}

				throw std::runtime_error("Invalid option passed to http request header, key " + key + " is suppose to contain a number/string/boolean");
			}
		}

		// delimiter
		ss << "\r\n\r\n";

		// body
		if (m_body_data.size() != 0)
			ss << std::string(m_body_data.begin(), m_body_data.end());

		return ss.str();
	}
}