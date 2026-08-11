#pragma once

#include "password_kdf.hpp"
#include "rate_limit_policy.hpp"

#include <cstddef>
#include <filesystem>

namespace credentials {

struct service_config {
    std::filesystem::path storage_path;
    // Equivalent to libsodium's crypto_pwhash_OPSLIMIT/MEMLIMIT_MODERATE.
    // Deliberately expensive: see docs/credentials-service.md's performance rule.
    kdf_parameters kdf{.ops_limit = 3, .mem_limit = 256U * 1024U * 1024U};
    rate_limit_policy rate_limit{};
    std::size_t min_secret_length{8};
    std::size_t max_principal_length{256};
};

} // namespace credentials
