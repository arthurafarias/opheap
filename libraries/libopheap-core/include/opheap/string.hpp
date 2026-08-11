#pragma once

#include "opheap/observer.hpp"

#include <memory_resource>
#include <string>
#include <string_view>

namespace opheap {

class string : public observable {
public:
    explicit string(std::pmr::memory_resource* resource = std::pmr::get_default_resource())
        : value_(resource), resource_(resource) {}

    string(std::string_view value,
           std::pmr::memory_resource* resource = std::pmr::get_default_resource())
        : value_(value, resource), resource_(resource) {}

    string(const char* value,
           std::pmr::memory_resource* resource = std::pmr::get_default_resource())
        : string(std::string_view{value}, resource) {}

    string(const string& other)
        : value_(other.value_, other.resource_), resource_(other.resource_) {}

    string(string&& other) noexcept
        : value_(std::move(other.value_), other.resource_), resource_(other.resource_) {}

    string& operator=(std::string_view value) {
        if (value_ != value) {
            value_.assign(value);
            notify(change_kind::value, 0, value_.size());
        }
        return *this;
    }

    string& operator=(const char* value) { return *this = std::string_view{value}; }

    string& operator=(const string& other) {
        return *this = other.view();
    }

    void append(std::string_view value) {
        value_.append(value);
        notify(change_kind::value, 0, value_.size());
    }

    void push_back(char value) {
        value_.push_back(value);
        notify(change_kind::structure, 0, value_.size());
    }

    void clear() noexcept {
        if (!value_.empty()) {
            value_.clear();
            notify(change_kind::structure);
        }
    }

    void erase(std::size_t pos, std::size_t count = std::pmr::string::npos) {
        value_.erase(pos, count);
        notify(change_kind::structure, pos, count);
    }

    [[nodiscard]] const char* c_str() const noexcept { return value_.c_str(); }
    [[nodiscard]] std::size_t size() const noexcept { return value_.size(); }
    [[nodiscard]] bool empty() const noexcept { return value_.empty(); }
    [[nodiscard]] std::string_view view() const noexcept { return {value_.data(), value_.size()}; }
    [[nodiscard]] std::pmr::memory_resource* resource() const noexcept { return resource_; }

    operator std::string_view() const noexcept { return view(); }

    friend bool operator==(const string& lhs, const string& rhs) noexcept {
        return lhs.view() == rhs.view();
    }
    friend bool operator==(const string& lhs, std::string_view rhs) noexcept {
        return lhs.view() == rhs;
    }

private:
    std::pmr::string value_;
    std::pmr::memory_resource* resource_;
};

} // namespace opheap
