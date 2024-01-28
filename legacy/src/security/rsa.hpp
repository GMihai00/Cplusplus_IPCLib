#pragma once

#include <iostream>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <memory>
#include <optional>
#include <vector>


namespace security
{
	class rsa_wrapper
	{
	private:
		std::string m_public_key_str;
		RSA* m_key_pair;
		bool m_is_private_key_available = false;

		RSA* generate_key_pair(std::string& public_key);
		RSA* read_public_key_from_string(const std::string& public_key_str);
	public:
		rsa_wrapper();
		rsa_wrapper(const std::string& publicKeyStr);
		~rsa_wrapper() noexcept;

		std::optional<std::vector<uint8_t>> encrypt_message(const std::vector<uint8_t>& message);
		std::optional<std::vector<uint8_t>> decrypt_message(const std::vector<uint8_t>& message);

		std::string get_public_key_as_string();
	};
} // namespace security
