#include "credential_service.hpp"

#include <algorithm>
#include <chrono>

namespace credentials {

namespace {

std::int64_t now_epoch_seconds() {
    return static_cast<std::int64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
}

} // namespace

credential_service::credential_service(service_config config)
    : heap_(opheap::heap::open(opheap::heap_config{.path = config.storage_path})),
      config_(std::move(config)) {}

bool credential_service::allow_attempt(std::string_view client_key) {
    static constexpr std::size_t prune_threshold = 4096;

    std::lock_guard lock{rate_mutex_};
    const auto now = std::chrono::steady_clock::now();

    if (rate_state_.size() > prune_threshold) {
        std::erase_if(rate_state_, [&](const auto& entry) {
            return now - entry.second.window_start > config_.rate_limit.window;
        });
    }

    auto& window = rate_state_[std::string{client_key}];
    if (now - window.window_start > config_.rate_limit.window) {
        window.window_start = now;
        window.count = 0;
    }
    if (window.count >= config_.rate_limit.max_attempts) return false;
    ++window.count;
    return true;
}

provision_status credential_service::provision(std::string_view principal, std::string_view secret,
                                                bool disabled, std::string_view client_key) {
    // Administrative operations get their own rate-limit bucket ("admin:"
    // prefix) so a public brute-force attempt against verify() cannot starve
    // a legitimate administrator sharing the same client_key (e.g. IP).
    if (!allow_attempt("admin:" + std::string{client_key})) return provision_status::rate_limited;
    return provision_locked(principal, secret, disabled);
}

provision_status credential_service::provision_locked(std::string_view principal, std::string_view secret,
                                                       bool disabled) {
    if (principal.empty() || principal.size() > config_.max_principal_length) {
        return provision_status::invalid_principal;
    }
    if (secret.size() < config_.min_secret_length) {
        return provision_status::invalid_secret;
    }

    const auto encoded = hash_secret(secret, config_.kdf);

    auto tx = heap_.begin();
    auto& root = tx.object_root();
    auto& credentials_root = root["credentials"].as_object();

    std::int64_t version = 1;
    std::int64_t created_at = now_epoch_seconds();
    if (auto existing = credentials_root.find(std::string{principal});
        existing != credentials_root.end() && existing->second.is_object()) {
        const auto& previous = existing->second.as_object();
        if (auto previous_version = previous.find("credential_version");
            previous_version != previous.end() && previous_version->second.is_integer()) {
            version = previous_version->second.as_integer() + 1;
        }
        if (auto previous_created = previous.find("created_at");
            previous_created != previous.end() && previous_created->second.is_integer()) {
            created_at = previous_created->second.as_integer();
        }
    }

    auto& record = credentials_root[std::string{principal}].as_object();
    record["scheme"] = "argon2id";
    record["digest"] = std::string_view{encoded};
    auto& parameters = record["parameters"].as_object();
    parameters["ops_limit"] = static_cast<std::int64_t>(config_.kdf.ops_limit);
    parameters["mem_limit"] = static_cast<std::int64_t>(config_.kdf.mem_limit);
    record["credential_version"] = version;
    record["disabled"] = disabled;
    record["created_at"] = created_at;
    record["updated_at"] = now_epoch_seconds();

    tx.commit();
    return provision_status::ok;
}

verify_status credential_service::verify(std::string_view principal, std::string_view secret,
                                         std::string_view client_key) {
    if (!allow_attempt(client_key)) return verify_status::rate_limited;

    std::string digest;
    {
        auto tx = heap_.begin();
        const auto& root = tx.object_root();
        if (auto credentials_root = root.find("credentials"); credentials_root != root.end() &&
            credentials_root->second.is_object()) {
            const auto& credentials_map = credentials_root->second.as_object();
            if (auto record = credentials_map.find(std::string{principal});
                record != credentials_map.end() && record->second.is_object()) {
                const auto& fields = record->second.as_object();
                const bool disabled = [&] {
                    auto it = fields.find("disabled");
                    return it != fields.end() && it->second.is_bool() && it->second.as_bool();
                }();
                if (!disabled) {
                    if (auto stored_digest = fields.find("digest");
                        stored_digest != fields.end() && stored_digest->second.is_string()) {
                        digest = std::string{stored_digest->second.as_string().view()};
                    }
                }
            }
        }
        // Read-only lookup: never persist, even if a missing "credentials"
        // root was materialized while walking the tree above.
        tx.abort();
    }

    if (digest.empty()) {
        perform_decoy_verification(secret, config_.kdf);
        return verify_status::rejected;
    }

    if (!verify_secret(digest, secret)) return verify_status::rejected;

    if (needs_rehash(digest, config_.kdf)) {
        // Opportunistic upgrade: only runs after a successful login, under
        // the same rate-limited budget already consumed above.
        provision_locked(principal, secret, false);
    }

    return verify_status::ok;
}

remove_status credential_service::remove(std::string_view principal, std::string_view client_key) {
    if (!allow_attempt("admin:" + std::string{client_key})) return remove_status::rate_limited;

    auto tx = heap_.begin();
    auto& root = tx.object_root();
    if (auto credentials_root = root.find("credentials"); credentials_root != root.end() &&
        credentials_root->second.is_object()) {
        auto& credentials_map = credentials_root->second.as_object();
        if (auto record = credentials_map.find(std::string{principal});
            record != credentials_map.end() && record->second.is_object()) {
            auto& fields = record->second.as_object();
            fields["disabled"] = true;
            fields["updated_at"] = now_epoch_seconds();
            tx.commit();
            return remove_status::ok;
        }
    }
    // Do not distinguish "did not exist" from "disabled" in the response.
    tx.abort();
    return remove_status::ok;
}

void credential_service::checkpoint() { heap_.checkpoint(); }

std::size_t credential_service::root_count() const noexcept { return heap_.root_count(); }

} // namespace credentials
