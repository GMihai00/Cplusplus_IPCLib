#pragma once

#include <string>
#include <optional>

#include <nlohmann/json.hpp>
#include <boost/asio.hpp>

#include "web_request_helpers.hpp"

#include "../utile/generic_error.hpp"

namespace net
{
	class ihttp_message
	{
	public:

		virtual std::string to_string() const = 0;
		virtual bool load_header_prefix(std::istringstream& iss) noexcept = 0;

		boost::asio::streambuf& get_buffer();
		nlohmann::json get_header() const;
		std::vector<uint8_t> get_body_raw() const;
		std::vector<uint8_t> get_body_decrypted() const;
		nlohmann::json get_json_body() const;

		utile::gzip_error gzip_compress_body();

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

	protected:

		std::string extract_header_from_buffer();

		boost::asio::streambuf m_buffer;
		nlohmann::json m_header_data = nullptr;
		std::vector<uint8_t> m_body_data;
	};
}