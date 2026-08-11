#pragma once

#include "opheap/observer.hpp"

#include <functional>
#include <map>
#include <memory_resource>
#include <type_traits>
#include <utility>

namespace opheap {

template<class K, detail::bindable V, class Compare = std::less<>>
struct map : public observable {
public:
    using key_type = K;
    using mapped_type = V;
    using container_type = std::pmr::map<K, V, Compare>;
    using iterator = typename container_type::iterator;
    using const_iterator = typename container_type::const_iterator;

    explicit map(std::pmr::memory_resource* resource = std::pmr::get_default_resource())
        : values_(Compare{}, resource), resource_(resource) {}

    map(const map& other)
        : values_(other.values_, other.resource_), resource_(other.resource_) {}

    map(map&& other) noexcept
        : values_(std::move(other.values_), other.resource_), resource_(other.resource_) {}

    void bind(binding b) noexcept {
        observable::bind(b);
        for (auto& [key, value] : values_) {
            (void)key;
            detail::bind_if_supported(value, b);
        }
    }

    V& operator[](const K& key) {
        auto [it, inserted] = values_.try_emplace(key);
        if (inserted) {
            detail::bind_if_supported(it->second, observer_binding());
            notify(change_kind::structure);
        }
        return it->second;
    }

    V& operator[](K&& key) {
        auto [it, inserted] = values_.try_emplace(std::move(key));
        if (inserted) {
            detail::bind_if_supported(it->second, observer_binding());
            notify(change_kind::structure);
        }
        return it->second;
    }

    template<class... Args>
    std::pair<iterator, bool> try_emplace(const K& key, Args&&... args) {
        auto result = values_.try_emplace(key, std::forward<Args>(args)...);
        if (result.second) {
            detail::bind_if_supported(result.first->second, observer_binding());
            notify(change_kind::structure);
        }
        return result;
    }

    std::size_t erase(const K& key) {
        const auto count = values_.erase(key);
        if (count != 0) notify(change_kind::structure);
        return count;
    }

    void clear() noexcept {
        if (!values_.empty()) {
            values_.clear();
            notify(change_kind::structure);
        }
    }

    iterator find(const K& key) { return values_.find(key); }
    const_iterator find(const K& key) const { return values_.find(key); }
    V& at(const K& key) { return values_.at(key); }
    const V& at(const K& key) const { return values_.at(key); }
    [[nodiscard]] bool contains(const K& key) const { return values_.contains(key); }
    [[nodiscard]] std::size_t size() const noexcept { return values_.size(); }
    [[nodiscard]] bool empty() const noexcept { return values_.empty(); }
    [[nodiscard]] std::pmr::memory_resource* resource() const noexcept { return resource_; }

    iterator begin() noexcept { return values_.begin(); }
    iterator end() noexcept { return values_.end(); }
    const_iterator begin() const noexcept { return values_.begin(); }
    const_iterator end() const noexcept { return values_.end(); }
    const_iterator cbegin() const noexcept { return values_.cbegin(); }
    const_iterator cend() const noexcept { return values_.cend(); }

private:
    container_type values_;
    std::pmr::memory_resource* resource_;
};

} // namespace opheap
