#include "password_kdf.hpp"

#include <sodium.h>

#include <mutex>
#include <stdexcept>

namespace credentials {

namespace {

void ensure_sodium_initialized() {
    static const int result = sodium_init();
    if (result < 0) throw std::runtime_error("libsodium initialization failed");
}

} // namespace

std::string hash_secret(std::string_view secret, const kdf_parameters& params) {
    ensure_sodium_initialized();
    std::string encoded(crypto_pwhash_STRBYTES, '\0');
    if (crypto_pwhash_str(encoded.data(), secret.data(), secret.size(),
                           params.ops_limit, params.mem_limit) != 0) {
        throw std::runtime_error("Argon2id hashing failed (likely out of memory)");
    }
    encoded.resize(std::char_traits<char>::length(encoded.c_str()));
    return encoded;
}

bool verify_secret(std::string_view encoded_hash, std::string_view secret) {
    ensure_sodium_initialized();
    const std::string encoded{encoded_hash};
    return crypto_pwhash_str_verify(encoded.c_str(), secret.data(), secret.size()) == 0;
}

bool needs_rehash(std::string_view encoded_hash, const kdf_parameters& params) {
    ensure_sodium_initialized();
    const std::string encoded{encoded_hash};
    return crypto_pwhash_str_needs_rehash(encoded.c_str(), params.ops_limit, params.mem_limit) == 1;
}

void perform_decoy_verification(std::string_view secret, const kdf_parameters& params) {
    ensure_sodium_initialized();
    static std::once_flag once;
    static std::string decoy_encoded;
    std::call_once(once, [&params] {
        decoy_encoded = hash_secret("opheap-beast-credentials-example-decoy", params);
    });
    [[maybe_unused]] const int result =
        crypto_pwhash_str_verify(decoy_encoded.c_str(), secret.data(), secret.size());
}

} // namespace credentials
