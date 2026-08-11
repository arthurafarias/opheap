#pragma once

#include "opheap/map.hpp"
#include "opheap/string.hpp"
#include "opheap/types.hpp"
#include "opheap/vector.hpp"

#include <cstdint>
#include <memory>
#include <memory_resource>
#include <string>
#include <string_view>
#include <variant>

namespace opheap {

class array;
class object;

class value : public observable {
public:
    using array_ptr = std::shared_ptr<array>;
    using object_ptr = std::shared_ptr<object>;
    using variant_type = std::variant<null_t, bool, std::int64_t, double, string, array_ptr, object_ptr>;

    explicit value(std::pmr::memory_resource* resource = std::pmr::get_default_resource())
        : data_(null), resource_(resource) {}
    value(null_t, std::pmr::memory_resource* resource = std::pmr::get_default_resource())
        : data_(null), resource_(resource) {}
    value(bool v, std::pmr::memory_resource* resource = std::pmr::get_default_resource())
        : data_(v), resource_(resource) {}
    value(int v, std::pmr::memory_resource* resource = std::pmr::get_default_resource())
        : data_(static_cast<std::int64_t>(v)), resource_(resource) {}
    value(std::int64_t v, std::pmr::memory_resource* resource = std::pmr::get_default_resource())
        : data_(v), resource_(resource) {}
    value(double v, std::pmr::memory_resource* resource = std::pmr::get_default_resource())
        : data_(v), resource_(resource) {}
    value(std::string_view v, std::pmr::memory_resource* resource = std::pmr::get_default_resource())
        : data_(string{v, resource}), resource_(resource) {}
    value(const char* v, std::pmr::memory_resource* resource = std::pmr::get_default_resource())
        : value(std::string_view{v}, resource) {}

    value(const value& other);
    value(value&& other) noexcept = default;
    value& operator=(const value& other);
    value& operator=(value&& other) noexcept;

    value& operator=(null_t);
    value& operator=(bool v);
    value& operator=(int v) { return *this = static_cast<std::int64_t>(v); }
    value& operator=(std::int64_t v);
    value& operator=(double v);
    value& operator=(std::string_view v);
    value& operator=(const char* v) { return *this = std::string_view{v}; }

    void bind(binding b) noexcept;

    [[nodiscard]] bool is_null() const noexcept { return std::holds_alternative<null_t>(data_); }
    [[nodiscard]] bool is_bool() const noexcept { return std::holds_alternative<bool>(data_); }
    [[nodiscard]] bool is_integer() const noexcept { return std::holds_alternative<std::int64_t>(data_); }
    [[nodiscard]] bool is_number() const noexcept { return std::holds_alternative<double>(data_); }
    [[nodiscard]] bool is_string() const noexcept { return std::holds_alternative<string>(data_); }
    [[nodiscard]] bool is_array() const noexcept { return std::holds_alternative<array_ptr>(data_); }
    [[nodiscard]] bool is_object() const noexcept { return std::holds_alternative<object_ptr>(data_); }

    [[nodiscard]] bool as_bool() const;
    [[nodiscard]] std::int64_t as_integer() const;
    [[nodiscard]] double as_number() const;
    [[nodiscard]] const string& as_string() const;
    string& as_string();
    array& as_array();
    const array& as_array() const;
    object& as_object();
    const object& as_object() const;

    value& operator[](std::string_view key);
    const value& at(std::string_view key) const;

    [[nodiscard]] std::pmr::memory_resource* resource() const noexcept { return resource_; }
    [[nodiscard]] const variant_type& storage() const noexcept { return data_; }

private:
    template<class T>
    void replace(T&& v, change_kind kind = change_kind::value) {
        data_ = std::forward<T>(v);
        bind(observer_binding());
        notify(kind);
    }

    variant_type data_;
    std::pmr::memory_resource* resource_;
};

class array final : public vector<value> {
public:
    using vector<value>::vector;
};

class object final : public map<std::string, value> {
public:
    using map<std::string, value>::map;
};

inline value::value(const value& other) : resource_(other.resource_) {
    if (std::holds_alternative<array_ptr>(other.data_)) {
        data_ = std::make_shared<array>(*std::get<array_ptr>(other.data_));
    } else if (std::holds_alternative<object_ptr>(other.data_)) {
        data_ = std::make_shared<object>(*std::get<object_ptr>(other.data_));
    } else {
        data_ = other.data_;
    }
}

inline value& value::operator=(const value& other) {
    if (this == &other) return *this;
    resource_ = other.resource_;
    if (std::holds_alternative<array_ptr>(other.data_)) {
        data_ = std::make_shared<array>(*std::get<array_ptr>(other.data_));
    } else if (std::holds_alternative<object_ptr>(other.data_)) {
        data_ = std::make_shared<object>(*std::get<object_ptr>(other.data_));
    } else {
        data_ = other.data_;
    }
    bind(observer_binding());
    notify(change_kind::value);
    return *this;
}


inline value& value::operator=(value&& other) noexcept {
    if (this == &other) return *this;
    const auto current_binding = observer_binding();
    data_ = std::move(other.data_);
    // Keep the destination allocator domain when it already belongs to a persistent graph.
    if (current_binding.observer == nullptr) resource_ = other.resource_;
    bind(current_binding);
    notify(change_kind::value);
    return *this;
}

inline value& value::operator=(null_t) { replace(null); return *this; }
inline value& value::operator=(bool v) { replace(v); return *this; }
inline value& value::operator=(std::int64_t v) { replace(v); return *this; }
inline value& value::operator=(double v) { replace(v); return *this; }
inline value& value::operator=(std::string_view v) { replace(string{v, resource_}); return *this; }

inline void value::bind(binding b) noexcept {
    observable::bind(b);
    std::visit([b](auto& item) {
        using item_type = std::remove_cvref_t<decltype(item)>;
        if constexpr (std::same_as<item_type, string>) {
            item.bind(b);
        } else if constexpr (std::same_as<item_type, array_ptr> || std::same_as<item_type, object_ptr>) {
            if (item) item->bind(b);
        }
    }, data_);
}

inline bool value::as_bool() const {
    if (!is_bool()) throw type_error("value is not bool");
    return std::get<bool>(data_);
}
inline std::int64_t value::as_integer() const {
    if (!is_integer()) throw type_error("value is not integer");
    return std::get<std::int64_t>(data_);
}
inline double value::as_number() const {
    if (!is_number()) throw type_error("value is not number");
    return std::get<double>(data_);
}
inline const string& value::as_string() const {
    if (!is_string()) throw type_error("value is not string");
    return std::get<string>(data_);
}
inline string& value::as_string() {
    if (!is_string()) replace(string{resource_});
    return std::get<string>(data_);
}
inline array& value::as_array() {
    if (!is_array()) replace(std::make_shared<array>(resource_), change_kind::structure);
    return *std::get<array_ptr>(data_);
}
inline const array& value::as_array() const {
    if (!is_array()) throw type_error("value is not array");
    return *std::get<array_ptr>(data_);
}
inline object& value::as_object() {
    if (!is_object()) replace(std::make_shared<object>(resource_), change_kind::structure);
    return *std::get<object_ptr>(data_);
}
inline const object& value::as_object() const {
    if (!is_object()) throw type_error("value is not object");
    return *std::get<object_ptr>(data_);
}
inline value& value::operator[](std::string_view key) {
    return as_object()[std::string{key}];
}
inline const value& value::at(std::string_view key) const {
    const auto& o = as_object();
    auto it = o.find(std::string{key});
    if (it == o.end()) throw std::out_of_range("opheap object key not found");
    return it->second;
}

} // namespace opheap
