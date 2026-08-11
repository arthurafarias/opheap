#pragma once

#include "opheap/module/sql/literal.hpp"
#include "opheap/module/sql/sql_error.hpp"

#include <opheap/value.hpp>

#include <optional>
#include <string>
#include <variant>

namespace opheap::module::sql {

// Returns -1/0/1, or nullopt when either side is NULL or the types cannot be compared.
[[nodiscard]] inline std::optional<int> compare_value_literal(const opheap::value& v, const literal& lit) {
    if (v.is_null() || std::holds_alternative<std::monostate>(lit)) return std::nullopt;

    if (v.is_string() && std::holds_alternative<std::string>(lit)) {
        const auto a = v.as_string().view();
        const auto& b = std::get<std::string>(lit);
        if (a < b) return -1;
        if (a > b) return 1;
        return 0;
    }
    if (v.is_bool() && std::holds_alternative<bool>(lit)) {
        const bool a = v.as_bool();
        const bool b = std::get<bool>(lit);
        if (a == b) return 0;
        return a ? 1 : -1;
    }
    if ((v.is_integer() || v.is_number()) &&
        (std::holds_alternative<std::int64_t>(lit) || std::holds_alternative<double>(lit))) {
        const double a = v.is_integer() ? static_cast<double>(v.as_integer()) : v.as_number();
        const double b = std::holds_alternative<std::int64_t>(lit)
            ? static_cast<double>(std::get<std::int64_t>(lit))
            : std::get<double>(lit);
        if (a < b) return -1;
        if (a > b) return 1;
        return 0;
    }
    throw sql_error("incomparable types in expression");
}

} // namespace opheap::module::sql
