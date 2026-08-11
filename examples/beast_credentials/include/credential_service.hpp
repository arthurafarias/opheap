#pragma once

#include "provision_status.hpp"
#include "remove_status.hpp"
#include "service_config.hpp"
#include "verify_status.hpp"

#include <opheap/opheap.hpp>

#include <chrono>
#include <cstddef>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

// Domain service for the Boost.Beast credentials example. This header and
// its implementation depend on opheap and libsodium only -- no Boost, no
// HTTP concepts -- matching the layering described in
// docs/credentials-service.md: the HTTP transport is a client of this
// service, never the other way around.
namespace credentials {

struct credential_service {
public:
    explicit credential_service(service_config config);

    credential_service(const credential_service&) = delete;
    credential_service& operator=(const credential_service&) = delete;
    credential_service(credential_service&&) = delete;
    credential_service& operator=(credential_service&&) = delete;

    // Provisions or replaces the credential for `principal`. `client_key`
    // identifies the caller for abuse-policy accounting (typically the
    // administrative caller's address).
    provision_status provision(std::string_view principal, std::string_view secret,
                                bool disabled, std::string_view client_key);

    // Verifies a presented secret. Never reveals whether a failure was
    // caused by a missing principal, a disabled credential, or a bad secret.
    verify_status verify(std::string_view principal, std::string_view secret,
                          std::string_view client_key);

    // Disables (soft-deletes) a credential. Returns `ok` even when the
    // principal does not exist, so callers cannot use this endpoint to probe
    // the credential set.
    remove_status remove(std::string_view principal, std::string_view client_key);

    void checkpoint();
    [[nodiscard]] std::size_t root_count() const noexcept;

private:
    struct attempt_window {
        std::size_t count{0};
        std::chrono::steady_clock::time_point window_start{};
    };

    [[nodiscard]] bool allow_attempt(std::string_view client_key);
    provision_status provision_locked(std::string_view principal, std::string_view secret, bool disabled);

    opheap::heap heap_;
    service_config config_;
    std::mutex rate_mutex_;
    std::unordered_map<std::string, attempt_window> rate_state_;
};

} // namespace credentials
