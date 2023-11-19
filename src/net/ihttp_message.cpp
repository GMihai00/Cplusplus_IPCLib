#include "ihttp_message.hpp"

#include <iostream>
#include <zlib.h>

namespace net
{
	namespace details
	{
		std::vector<uint8_t> decompress(const std::vector<uint8_t>& compressedData) {
			std::vector<uint8_t> decompressedData;

			z_stream stream;
			stream.zalloc = Z_NULL;
			stream.zfree = Z_NULL;
			stream.opaque = Z_NULL;
			stream.avail_in = 0;
			stream.next_in = Z_NULL;

			int ret = inflateInit2(&stream, 16 + MAX_WBITS); // Use zlib format with gzip wrapper
			if (ret != Z_OK) {
				throw std::runtime_error("Failed to initialize zlib");
			}

			stream.avail_in = static_cast<uInt>(compressedData.size());
			stream.next_in = reinterpret_cast<Bytef*>(const_cast<uint8_t*>(compressedData.data()));

			const size_t bufferSize = 4096;
			std::vector<uint8_t> buffer(bufferSize);

			do {
				stream.avail_out = bufferSize;
				stream.next_out = reinterpret_cast<Bytef*>(buffer.data());
				ret = inflate(&stream, Z_NO_FLUSH);

				switch (ret) {
				case Z_NEED_DICT:
				case Z_DATA_ERROR:
				case Z_MEM_ERROR:
					inflateEnd(&stream);
					throw std::runtime_error("Decompression error");
				}

				size_t have = bufferSize - stream.avail_out;
				decompressedData.insert(decompressedData.end(), buffer.begin(), buffer.begin() + have);

			} while (stream.avail_out == 0);

			inflateEnd(&stream);

			return decompressedData;
		}
	}

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

	std::vector<uint8_t> ihttp_message::get_body_decrypted() const
	{
		if (auto it = m_header_data.find("Content-Encoding"); it != m_header_data.end() && it->is_string())
		{
			auto encoding_type = it->get<std::string>();

			// only gzip support for now
			if (encoding_type.find("gzip") != std::string::npos)
				return details::decompress(m_body_data);
		}

		return m_body_data;
	}

	nlohmann::json ihttp_message::get_json_body() const
	{
		if (!m_body_data.empty())
		{
			auto decrypted_body = get_body_decrypted();
			return nlohmann::json::parse(std::string(decrypted_body.begin(), decrypted_body.end()));
		}

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