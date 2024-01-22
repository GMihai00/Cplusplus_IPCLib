#include "gzip_helpers.hpp"

#include "zlib.h"
#include "finally.hpp"

namespace utile
{
	namespace gzip
	{
		std::vector<uint8_t> decompress(const std::vector<uint8_t>& compressed_data, gzip_error& err) noexcept {
			std::vector<uint8_t> decompressed_data;

			z_stream stream;
			stream.zalloc = Z_NULL;
			stream.zfree = Z_NULL;
			stream.opaque = Z_NULL;
			stream.avail_in = 0;
			stream.next_in = Z_NULL;

			int ret = inflateInit2(&stream, 16 + MAX_WBITS); // Use zlib format with gzip wrapper
			if (ret != Z_OK) {
				err = utile::gzip_error(std::error_code(ret, std::generic_category()), "Failed to initialize zlib");
				return compressed_data;
			}

			stream.avail_in = static_cast<uInt>(compressed_data.size());
			stream.next_in = reinterpret_cast<Bytef*>(const_cast<uint8_t*>(compressed_data.data()));

			const size_t buffer_size = 4096;
			std::vector<uint8_t> buffer(buffer_size);

			auto end_inflate = utile::finally([&stream]() {
					inflateEnd(&stream);
				});

			do {
				stream.avail_out = buffer_size;
				stream.next_out = reinterpret_cast<Bytef*>(buffer.data());
				ret = inflate(&stream, Z_NO_FLUSH);

				switch (ret) {
				case Z_NEED_DICT:
				case Z_DATA_ERROR:
				case Z_MEM_ERROR:
					
					err = utile::gzip_error(std::error_code(ret, std::generic_category()), "Decompression error");
					return compressed_data;
				}

				size_t have = buffer_size - stream.avail_out;
				decompressed_data.insert(decompressed_data.end(), buffer.begin(), buffer.begin() + have);

			} while (stream.avail_out == 0);

			return decompressed_data;
		}

		std::vector<uint8_t> compress(const std::vector<uint8_t>& input_data, gzip_error& err) noexcept
		{
			std::vector<uint8_t> compressed_data;

			z_stream deflate_stream;
			deflate_stream.zalloc = Z_NULL;
			deflate_stream.zfree = Z_NULL;
			deflate_stream.opaque = Z_NULL;
			deflate_stream.avail_in = 0;
			deflate_stream.next_in = Z_NULL;

			if (auto ret = deflateInit2(&deflate_stream, Z_BEST_COMPRESSION, Z_DEFLATED, 16 + MAX_WBITS, 8, Z_DEFAULT_STRATEGY); ret != Z_OK) {
				err = utile::gzip_error(std::error_code(ret, std::generic_category()), "Error initializing deflate stream.");
				return input_data;
			}

			const size_t buffer_size = 4096;
			std::vector<uint8_t> buffer(buffer_size);

			deflate_stream.avail_in = static_cast<uInt>(input_data.size());
			deflate_stream.next_in = reinterpret_cast<Bytef*>(const_cast<uint8_t*>(input_data.data()));

			auto end_deflate = utile::finally([&deflate_stream]() {
				deflateEnd(&deflate_stream);
				});

			do {
				deflate_stream.avail_out = buffer_size;
				deflate_stream.next_out = reinterpret_cast<Bytef*>(buffer.data());

				if (auto ret = deflate(&deflate_stream, Z_FINISH); ret == Z_STREAM_ERROR) {
					err = utile::gzip_error(std::error_code(ret, std::generic_category()), "Error compressing data.");
					return input_data;
				}

				size_t compressedSize = buffer_size - deflate_stream.avail_out;
				compressed_data.insert(compressed_data.end(), buffer.begin(), buffer.begin() + compressedSize);

			} while (deflate_stream.avail_out == 0);

			return compressed_data;
		}

	}
}