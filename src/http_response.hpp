#pragma once

#include <string>
#include <nlohmann/json.hpp>
#include <boost/asio.hpp>
#include <optional>
namespace net
{
	class http_response
	{
	public:
		boost::asio::streambuf& get_buffer();
		nlohmann::json get_header() const;
		std::vector<uint8_t> get_body_raw() const;
		nlohmann::json get_json_body() const;

		template <typename T>
		std::optional<T> get_header_value(const std::string& name) try
		{
			if (auto it = m_header_data.find(name); it != m_header_data.end())
			{
				return it->get<T>();
			}

			return std::nullopt;
		}
		catch (...)
		{
			return std::nullopt;
		}

		bool build_header_from_data_recieved();
		void finalize_message();
	private:
		boost::asio::streambuf m_buffer;
		std::string m_version;
		uint16_t m_status;
		std::string m_reason;
		nlohmann::json m_header_data = nlohmann::json("{}");
		std::vector<uint8_t> m_body_data;
	};
}