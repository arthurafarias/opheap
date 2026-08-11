#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace credentials {

// Argon2id cost parameters. Defaults are chosen by the caller; see
// service_config in credential_service.hpp for the values this example uses.
struct kdf_parameters {
    unsigned long long ops_limit{};
    std::size_t mem_limit{};
};

// Hashes `secret` with Argon2id (libsodium). The returned PHC-formatted
// string embeds the algorithm, salt and cost parameters, so verification
// never needs the original kdf_parameters back.
std::string hash_secret(std::string_view secret, const kdf_parameters& params);

// Constant-time verification of `secret` against a previously encoded hash.
bool verify_secret(std::string_view encoded_hash, std::string_view secret);

// True when `encoded_hash` was produced with weaker parameters than `params`,
// signalling that the caller should rehash on the next successful login.
bool needs_rehash(std::string_view encoded_hash, const kdf_parameters& params);

// Runs a full Argon2id verification against a fixed, lazily-generated decoy
// hash so that "unknown principal" and "wrong password" cost the same amount
// of CPU time on average. Call this whenever verification fails to find a
// digest to compare against, instead of returning early.
void perform_decoy_verification(std::string_view secret, const kdf_parameters& params);

} // namespace credentials
