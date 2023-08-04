#include "RSA.hpp"

#pragma warning(disable : 4996)


namespace security
{
    RSA* rsa_wrapper::generate_key_pair(std::string& publicKey)
    {
        RSA* rsaKeyPair = nullptr;
        BIGNUM* bne = nullptr;
        BIO* privateKeyBio = nullptr;
        BIO* publicKeyBio = nullptr;

        // Generate RSA key pair
        bne = BN_new();
        if (BN_set_word(bne, RSA_F4) != 1)
        {
            std::cerr << "Failed to set RSA exponent." << std::endl;
            return nullptr;
        }

        rsaKeyPair = RSA_new();
        if (RSA_generate_key_ex(rsaKeyPair, 2048, bne, nullptr) != 1)
        {
            std::cerr << "Failed to generate RSA key pair." << std::endl;
            RSA_free(rsaKeyPair);
            return nullptr;
        }

        // Get public key in PEM format
        publicKeyBio = BIO_new(BIO_s_mem());
        if (PEM_write_bio_RSAPublicKey(publicKeyBio, rsaKeyPair) != 1)
        {
            std::cerr << "Failed to write RSA public key." << std::endl;
            RSA_free(rsaKeyPair);
            BIO_free_all(publicKeyBio);
            return nullptr;
        }

        // Read public key from BIO into a string
        char* publicKeyBuffer = nullptr;
        long publicKeySize = BIO_get_mem_data(publicKeyBio, &publicKeyBuffer);
        publicKey.assign(publicKeyBuffer, publicKeySize);

        privateKeyBio = BIO_new(BIO_s_mem());
        if (PEM_write_bio_RSAPrivateKey(privateKeyBio, rsaKeyPair, nullptr, nullptr, 0, nullptr, nullptr) != 1)
        {
            std::cerr << "Failed to write RSA private key." << std::endl;
            RSA_free(rsaKeyPair);
            BIO_free_all(publicKeyBio);
            BIO_free_all(privateKeyBio);
            return nullptr;
        }

        // Clean up
        BN_free(bne);
        BIO_free_all(publicKeyBio);
        BIO_free_all(privateKeyBio);

        return rsaKeyPair;
    }

    RSA* rsa_wrapper::read_public_key_from_string(const std::string& public_key_str)
    {
        RSA* rsaPublicKey = nullptr;
        BIO* publicKeyBio = BIO_new_mem_buf(public_key_str.c_str(), -1);

        if (!publicKeyBio)
        {
            std::cerr << "Failed to create BIO for public key." << std::endl;
            return nullptr;
        }

        rsaPublicKey = PEM_read_bio_RSAPublicKey(publicKeyBio, nullptr, nullptr, nullptr);

        if (!rsaPublicKey)
        {
            std::cerr << "Failed to read public key from string." << std::endl;
            BIO_free_all(publicKeyBio);
            return nullptr;
        }

        BIO_free_all(publicKeyBio);

        return rsaPublicKey;
    }

    rsa_wrapper::rsa_wrapper()
    {
        m_key_pair = generate_key_pair(m_public_key_str);
        if (!m_key_pair)
        {
            throw std::runtime_error("Failed to generate RSA key pair.");
        }
        m_is_private_key_available = true;
    }

    rsa_wrapper::rsa_wrapper(const std::string& public_key_str) : m_public_key_str(public_key_str)
    {
        // in this scenario private key will be missing from pair
        m_key_pair = read_public_key_from_string(m_public_key_str);
        if (!m_key_pair)
        {
            throw std::runtime_error("Failed to acquire public key");
        }
    }

    rsa_wrapper::~rsa_wrapper() noexcept
    {
        RSA_free(m_key_pair);
    }

    std::optional<std::vector<uint8_t>> rsa_wrapper::encrypt_message(const std::vector<uint8_t>& message)
    {
        int encrypted_size = RSA_size(m_key_pair);
        std::vector<uint8_t> encrypted(encrypted_size);

        int result = RSA_public_encrypt(
            static_cast<int>(message.size()),
            message.data(),
            encrypted.data(),
            m_key_pair,
            RSA_PKCS1_PADDING
        );

        if (result == -1) {
            std::cerr << "Failed to encrypt the message." << std::endl;
            return std::nullopt;
        }

        return std::vector<uint8_t>(encrypted.begin(), encrypted.begin() + result);
    }

    std::optional<std::vector<uint8_t>> rsa_wrapper::decrypt_message(const std::vector<uint8_t>& message)
    {
        if (!m_is_private_key_available)
        {
            std::cerr << "Private key not present";
            return std::nullopt;
        }

        int decrypted_size = RSA_size(m_key_pair);
        std::vector<uint8_t> decrypted(decrypted_size);

        auto result = RSA_private_decrypt(
            static_cast<int>(message.size()),
            message.data(),
            decrypted.data(),
            m_key_pair,
            RSA_PKCS1_PADDING
        );

        if (result == -1) {
            std::cerr << "Failed to decrypt the message." << std::endl;
            return std::nullopt;
        }

        return std::vector<uint8_t>(decrypted.begin(), decrypted.begin() + result);
    }

    std::string rsa_wrapper::get_public_key_as_string()
    {
        return m_public_key_str;
    }

} // namespace security


#pragma warning(default : 4496)



