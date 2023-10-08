#include "http_response.hpp"
#include <iostream>

namespace net
{


	boost::asio::streambuf& http_response::get_buffer()
	{
		return m_buffer;
	}

	nlohmann::json http_response::get_header() const
	{
		return m_header_data;
	}

	std::vector<uint8_t> http_response::get_body_raw() const
	{
		return m_body_data;
	}

	nlohmann::json http_response::get_json_body() const
	{
		if (!m_body_data.empty())
			return nlohmann::json::parse(std::string(m_body_data.begin(), m_body_data.end()));
		
		return nlohmann::json();
	}

	bool http_response::build_header_from_data_recieved()
	{
		const char* data = boost::asio::buffer_cast<const char*>(m_buffer.data());
		std::size_t size = m_buffer.size();
		
		std::istringstream iss(std::string(data, size));
		
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

		while (std::getline(iss, line, '\r'))
		{
			if (!line.empty()) {
				
				if (auto it = line.find(": "); it != std::string::npos && it + 2 < line.size())
				{
					auto name = line.substr(0, it);
					auto value = line.substr(it + 2, line.size());
					try
					{
						auto int_value = std::stof(value);
						m_header_data.emplace(name, int_value);
					}
					catch (...)
					{
						m_header_data.emplace(name, value);
					}
				}

			}

			if (iss.get() != '\n') {
				break;
			}
		}

		if (!iss.eof())
		{
			std::cerr << "Invalid characters still left inside the header";
			return false;
		}

		m_buffer.consume(m_buffer.size());

		return true;
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

	void http_response::finalize_message()
	{
		const char* data = boost::asio::buffer_cast<const char*>(m_buffer.data());
		std::size_t size = m_buffer.size();

		auto string_data = std::string(data, size);

		if (!string_data.empty())
			m_body_data = std::vector<uint8_t>(string_data.begin(), string_data.end());
	}
}