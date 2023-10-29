#include "ihttp_message.hpp"

#include <iostream>

namespace net
{
	boost::asio::streambuf& ihttp_message::get_buffer()
	{
		return m_buffer;
	}

	nlohmann::json ihttp_message::get_header() const
	{
		return m_header_data;
	}

	std::vector<uint8_t> ihttp_message::get_body_raw() const
	{
		return m_body_data;
	}

	nlohmann::json ihttp_message::get_json_body() const
	{
		if (!m_body_data.empty())
			return nlohmann::json::parse(std::string(m_body_data.begin(), m_body_data.end()));

		return nlohmann::json();
	}

	std::string ihttp_message::extract_header_from_buffer()
	{
		const char* data = boost::asio::buffer_cast<const char*>(m_buffer.data());
		std::size_t size = m_buffer.size();
		const char* search_str = "\r\n\r\n";
		std::size_t search_len = std::strlen(search_str);

		std::size_t header_end = size - search_len;

		for (std::size_t i = 0; i < size - search_len + 1; ++i) {
			if (std::memcmp(data + i, search_str, search_len) == 0) {
				header_end = i;
				break;
			}
		}

		return std::string(data, header_end + search_len);
	}

	bool ihttp_message::build_header_from_data_recieved()
	{
		auto header = extract_header_from_buffer();

		m_buffer.consume(header.size());

		std::istringstream iss(header);

		if (!load_header_prefix(iss))
		{
			// for debug only
			std::cerr << "Failed to find load header prefix";
			return false;
		}

		std::string line;
		while (std::getline(iss, line, '\r'))
		{
			if (!line.empty()) {

				if (auto it = line.find(": "); it != std::string::npos && it + 2 < line.size())
				{
					size_t start = 0;
					size_t end = it;
					if (line[0] == '\n')
					{
						start = 1;
						end -= 1;
					}

					auto name = line.substr(start, end);
					auto value = line.substr(it + 2, line.size() - it - 2);
					try
					{
						auto int_value = std::stoull(value);

						if (std::to_string(int_value) != value)
						{
							m_header_data.emplace(name, value);
							continue;
						}

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

		return true;
	}

	void ihttp_message::finalize_message()
	{
		const char* data = boost::asio::buffer_cast<const char*>(m_buffer.data());
		std::size_t size = m_buffer.size();

		auto string_data = std::string(data, size);

		if (!string_data.empty())
			m_body_data = std::vector<uint8_t>(string_data.begin(), string_data.end());
	}
}