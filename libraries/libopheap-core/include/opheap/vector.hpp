#pragma once

#include "opheap/observer.hpp"

#include <cstddef>
#include <memory_resource>
#include <type_traits>
#include <utility>
#include <vector>

namespace opheap {

template<class T>
concept observable_element = detail::bindable<T>;

template<observable_element T>
struct vector : public observable {
public:
    using value_type = T;
    using container_type = std::pmr::vector<T>;
    using iterator = typename container_type::iterator;
    using const_iterator = typename container_type::const_iterator;

    explicit vector(std::pmr::memory_resource* resource = std::pmr::get_default_resource())
        : values_(resource), resource_(resource) {}

    vector(const vector& other)
        : values_(other.values_, other.resource_), resource_(other.resource_) {}

    vector(vector&& other) noexcept
        : values_(std::move(other.values_), other.resource_), resource_(other.resource_) {}

    void bind(binding b) noexcept {
        observable::bind(b);
        for (auto& value : values_) detail::bind_if_supported(value, b);
    }

    template<class... Args>
    T& emplace_back(Args&&... args) {
        const auto* old_data = values_.data();
        auto& value = values_.emplace_back(std::forward<Args>(args)...);
        if (old_data != values_.data()) rebind_all();
        else detail::bind_if_supported(value, observer_binding());
        notify(change_kind::structure);
        return value;
    }

    void push_back(const T& value) {
        const auto* old_data = values_.data();
        values_.push_back(value);
        if (old_data != values_.data()) rebind_all();
        else detail::bind_if_supported(values_.back(), observer_binding());
        notify(change_kind::structure);
    }

    void push_back(T&& value) {
        const auto* old_data = values_.data();
        values_.push_back(std::move(value));
        if (old_data != values_.data()) rebind_all();
        else detail::bind_if_supported(values_.back(), observer_binding());
        notify(change_kind::structure);
    }

    iterator erase(const_iterator pos) {
        auto result = values_.erase(pos);
        rebind_all();
        notify(change_kind::structure);
        return result;
    }

    void clear() noexcept {
        if (!values_.empty()) {
            values_.clear();
            notify(change_kind::structure);
        }
    }

    void reserve(std::size_t size) {
        const auto old = values_.capacity();
        values_.reserve(size);
        if (values_.capacity() != old) {
            rebind_all();
            notify(change_kind::allocation);
        }
    }

    [[nodiscard]] T& operator[](std::size_t index) noexcept { return values_[index]; }
    [[nodiscard]] const T& operator[](std::size_t index) const noexcept { return values_[index]; }
    [[nodiscard]] T& at(std::size_t index) { return values_.at(index); }
    [[nodiscard]] const T& at(std::size_t index) const { return values_.at(index); }
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
    void rebind_all() noexcept {
        const auto b = observer_binding();
        for (auto& value : values_) detail::bind_if_supported(value, b);
    }

    container_type values_;
    std::pmr::memory_resource* resource_;
};

} // namespace opheap
