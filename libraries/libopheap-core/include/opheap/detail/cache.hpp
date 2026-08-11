#pragma once

#include "opheap/types.hpp"

#include <cstddef>
#include <cstdint>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace opheap::detail {

struct payload_cache {
public:
    explicit payload_cache(std::size_t limit) : limit_(limit) {}

    [[nodiscard]] std::shared_ptr<const std::vector<std::byte>>
    get(std::string_view name, version_type version) {
        std::lock_guard lock{mutex_};
        const auto found = entries_.find(std::string{name});
        if (found == entries_.end() || found->second->version != version) {
            ++misses_;
            if (found != entries_.end()) erase(found->second);
            return {};
        }
        ++hits_;
        lru_.splice(lru_.begin(), lru_, found->second);
        return found->second->payload;
    }

    [[nodiscard]] std::shared_ptr<const std::vector<std::byte>>
    put(std::string name, version_type version, std::vector<std::byte> payload) {
        auto shared = std::make_shared<const std::vector<std::byte>>(std::move(payload));
        if (limit_ == 0 || shared->size() > limit_) return shared;

        std::lock_guard lock{mutex_};
        if (const auto found = entries_.find(name); found != entries_.end()) {
            erase(found->second);
        }
        while (!lru_.empty() && bytes_ + shared->size() > limit_) {
            erase(std::prev(lru_.end()));
            ++evictions_;
        }

        lru_.push_front(entry{std::move(name), version, shared});
        entries_.emplace(lru_.front().name, lru_.begin());
        bytes_ += shared->size();
        return shared;
    }

    [[nodiscard]] cache_info info() const noexcept {
        std::lock_guard lock{mutex_};
        return cache_info{bytes_, lru_.size(), hits_, misses_, evictions_};
    }

    void clear() noexcept {
        std::lock_guard lock{mutex_};
        entries_.clear();
        lru_.clear();
        bytes_ = 0;
    }

private:
    struct entry {
        std::string name;
        version_type version{};
        std::shared_ptr<const std::vector<std::byte>> payload;
    };

    using list_type = std::list<entry>;

    void erase(list_type::iterator it) {
        bytes_ -= it->payload->size();
        entries_.erase(it->name);
        lru_.erase(it);
    }

    std::size_t limit_{};
    mutable std::mutex mutex_;
    list_type lru_;
    std::unordered_map<std::string, list_type::iterator> entries_;
    std::size_t bytes_{};
    std::uint64_t hits_{};
    std::uint64_t misses_{};
    std::uint64_t evictions_{};
};

} // namespace opheap::detail
