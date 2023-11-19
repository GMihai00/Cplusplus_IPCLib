#pragma once

#include <vector>

#include "generic_error.hpp"

namespace utile
{
	namespace gzip
	{
		std::vector<uint8_t> decompress(const std::vector<uint8_t>& compressed_data, gzip_error& err) noexcept;
		std::vector<uint8_t> compress(const std::vector<uint8_t>& input_data, gzip_error& err) noexcept;
	}
}