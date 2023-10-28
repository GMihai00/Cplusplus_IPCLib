#include "http_response.hpp"
#include <iostream>

namespace net
{
	http_response::http_response(const uint16_t status, const std::string reason, const nlohmann::json& header_data,
		const std::vector<uint8_t>& body_data) : m_status(status), m_reason(reason)
	{
		m_header_data = header_data;
		m_body_data = body_data;
		m_header_data["Content-Length"] = body_data.size();
	}

	std::string http_response::to_string() const
	{
		std::stringstream ss;

		ss << m_version << " " << m_status << " " << m_reason << "\r\n";

		for (const auto& item : m_header_data.items())
		{
			const auto& val = item.value();

			if (val.is_number())
			{
				ss << item.key() << ": " << val.get<long long>() << "\r\n";
			}
			else if (val.is_string())
			{
				ss << item.key() << ": " << val.get<std::string>() << "\r\n";
			}
		}
		
		ss << "\r\n\r\n";

		ss << std::string(m_body_data.begin(), m_body_data.end());

		return ss.str();
	}

	bool http_response::load_header_prefix(std::istringstream& iss) noexcept try
	{
		std::string line;
		if (std::getline(iss, line, '\r'))
		{
			if (auto it = line.find(" "); it == std::string::npos)
			{
				return false;
			}
			else
			{
				m_version = line.substr(0, it);
				auto start = it + 1;
				it = line.find(" ", start);
				if (it == std::string::npos)
					return false;

				try
				{
					m_status = std::stol(line.substr(start, it));
				}
				catch (const std::exception& err)
				{
					std::cerr << "Failed to read return status err: " << err.what();
					return false;
				}

				m_reason = line.substr(it + 1, line.size());
			}
		}
		else
		{
			std::cerr << "Failed to find req answear";
			return false;
		}
		return true;
	}
	catch (const std::exception& err)
	{
		std::cerr << err.what();
		return false;
	}
	
	std::string http_response::get_version() const
	{
		return m_version;
	}

	uint16_t http_response::get_status() const
	{
		return m_status;
	}

	std::string http_response::get_reason() const
	{
		return m_reason;
	}
}