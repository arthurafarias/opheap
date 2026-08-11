// Self-contained smoke test for the credential_service domain layer.
// Exercises only opheap + libsodium, matching the example's layering: no
// Boost, no sockets, so it runs happily under `ctest`.
#include "credential_service.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void expect(bool condition, std::string_view message) {
    if (!condition) throw std::runtime_error(std::string{message});
}

std::filesystem::path make_storage_dir(std::string_view name) {
    static int counter = 0;
    auto path = std::filesystem::temp_directory_path() /
        ("opheap-beast-credentials-test-" + std::string{name} + "-" + std::to_string(counter++));
    std::filesystem::remove_all(path);
    return path;
}

credentials::service_config fast_test_config(std::filesystem::path storage_path) {
    // OPSLIMIT_MIN / a small memlimit: the test cares about correctness, not
    // about the KDF being expensive, so keep it fast.
    return credentials::service_config{
        .storage_path = std::move(storage_path),
        .kdf = {.ops_limit = 1, .mem_limit = 8U * 1024U * 1024U},
        .rate_limit = {.max_attempts = 3, .window = std::chrono::minutes{1}},
    };
}

void test_verify_accepts_correct_secret_only() {
    credentials::credential_service service{fast_test_config(make_storage_dir("verify"))};

    expect(service.provision("arthur", "correct horse battery staple", false, "admin") ==
               credentials::provision_status::ok,
           "provisioning a valid credential should succeed");

    expect(service.verify("arthur", "correct horse battery staple", "client-a") ==
               credentials::verify_status::ok,
           "verify should accept the correct secret");
    expect(service.verify("arthur", "wrong secret", "client-b") == credentials::verify_status::rejected,
           "verify should reject a wrong secret");
}

void test_verify_does_not_distinguish_unknown_principal() {
    credentials::credential_service service{fast_test_config(make_storage_dir("enumeration"))};
    expect(service.provision("known", "some long enough secret", false, "admin") ==
               credentials::provision_status::ok,
           "provisioning should succeed");

    // Both an unknown principal and a wrong secret for a known principal must
    // report the same status: anti-enumeration is the whole point.
    const auto unknown = service.verify("does-not-exist", "irrelevant secret", "client-a");
    const auto wrong = service.verify("known", "not the right secret", "client-b");
    expect(unknown == credentials::verify_status::rejected, "unknown principal should be rejected");
    expect(wrong == credentials::verify_status::rejected, "wrong secret should be rejected");
    expect(unknown == wrong, "unknown principal and wrong secret must be indistinguishable to the caller");
}

void test_disabled_credential_is_rejected() {
    credentials::credential_service service{fast_test_config(make_storage_dir("disabled"))};
    expect(service.provision("arthur", "correct horse battery staple", false, "admin") ==
               credentials::provision_status::ok,
           "provisioning should succeed");
    expect(service.remove("arthur", "admin") == credentials::remove_status::ok, "remove should succeed");

    expect(service.verify("arthur", "correct horse battery staple", "client-a") ==
               credentials::verify_status::rejected,
           "a disabled credential must reject even the correct secret");
}

void test_rate_limit_engages_after_max_attempts() {
    auto config = fast_test_config(make_storage_dir("rate-limit"));
    credentials::credential_service service{config};
    expect(service.provision("arthur", "correct horse battery staple", false, "admin") ==
               credentials::provision_status::ok,
           "provisioning should succeed");

    std::size_t rejected_count = 0;
    std::size_t rate_limited_count = 0;
    for (std::size_t attempt = 0; attempt < config.rate_limit.max_attempts + 2; ++attempt) {
        switch (service.verify("arthur", "wrong secret", "same-client")) {
            case credentials::verify_status::rejected: ++rejected_count; break;
            case credentials::verify_status::rate_limited: ++rate_limited_count; break;
            case credentials::verify_status::ok: expect(false, "wrong secret must never verify as ok"); break;
        }
    }
    expect(rate_limited_count > 0, "exceeding max_attempts within the window should trigger rate limiting");
    expect(rejected_count == config.rate_limit.max_attempts,
           "exactly max_attempts requests should be let through before rate limiting engages");
}

void test_credentials_persist_across_reopen() {
    const auto storage_path = make_storage_dir("persist");
    {
        credentials::credential_service service{fast_test_config(storage_path)};
        expect(service.provision("arthur", "correct horse battery staple", false, "admin") ==
                   credentials::provision_status::ok,
               "provisioning should succeed");
        service.checkpoint();
    }
    {
        credentials::credential_service service{fast_test_config(storage_path)};
        expect(service.verify("arthur", "correct horse battery staple", "client-a") ==
                   credentials::verify_status::ok,
               "credentials must survive a service restart");
    }
}

} // namespace

int main() {
    const std::pair<std::string_view, void (*)()> tests[] = {
        {"verify_accepts_correct_secret_only", test_verify_accepts_correct_secret_only},
        {"verify_does_not_distinguish_unknown_principal", test_verify_does_not_distinguish_unknown_principal},
        {"disabled_credential_is_rejected", test_disabled_credential_is_rejected},
        {"rate_limit_engages_after_max_attempts", test_rate_limit_engages_after_max_attempts},
        {"credentials_persist_across_reopen", test_credentials_persist_across_reopen},
    };

    std::size_t failed = 0;
    for (const auto& [name, test] : tests) {
        try {
            test();
            std::cout << "[PASS] " << name << '\n';
        } catch (const std::exception& error) {
            std::cerr << "[FAIL] " << name << " - " << error.what() << '\n';
            ++failed;
        }
    }
    std::cout << '\n' << (std::size(tests) - failed) << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}
