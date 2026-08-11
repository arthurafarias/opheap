#pragma once

#include "opheap/null_t.hpp"
#include "opheap/module/sql/column.hpp"
#include "opheap/module/sql/literal.hpp"
#include "opheap/module/sql/sql_error.hpp"
#include "opheap/value.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

namespace opheap::module::sql {

[[nodiscard]] inline bool contains_column(const std::vector<column_definition>& columns, std::string_view name) {
    return std::any_of(columns.begin(), columns.end(), [&](const auto& c) { return c.name == name; });
}

[[nodiscard]] inline literal to_literal(const opheap::value& v) {
    if (v.is_null()) return literal{std::monostate{}};
    if (v.is_bool()) return literal{v.as_bool()};
    if (v.is_integer()) return literal{v.as_integer()};
    if (v.is_number()) return literal{v.as_number()};
    if (v.is_string()) return literal{std::string{v.as_string().view()}};
    throw sql_error("value has no SQL-representable type");
}

inline void assign_literal(opheap::value& target, const literal& lit) {
    std::visit([&](auto&& v) {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, std::monostate>) target = opheap::null;
        else target = v;
    }, lit);
}

// NULL literals are always accepted regardless of declared column type.
[[nodiscard]] inline literal coerce(const literal& lit, column_type type, std::string_view column_name) {
    if (std::holds_alternative<std::monostate>(lit)) return lit;
    switch (type) {
        case column_type::integer:
            if (!std::holds_alternative<std::int64_t>(lit))
                throw sql_error("column '" + std::string{column_name} + "' expects INTEGER");
            return lit;
        case column_type::real:
            if (std::holds_alternative<std::int64_t>(lit)) return literal{static_cast<double>(std::get<std::int64_t>(lit))};
            if (!std::holds_alternative<double>(lit))
                throw sql_error("column '" + std::string{column_name} + "' expects REAL");
            return lit;
        case column_type::text:
            if (!std::holds_alternative<std::string>(lit))
                throw sql_error("column '" + std::string{column_name} + "' expects TEXT");
            return lit;
        case column_type::boolean:
            if (!std::holds_alternative<bool>(lit))
                throw sql_error("column '" + std::string{column_name} + "' expects BOOLEAN");
            return lit;
    }
    throw sql_error("unknown column type");
}

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

// NULLs sort after every non-NULL value, regardless of sort direction.
[[nodiscard]] inline bool less_for_sort(const opheap::value& a, const opheap::value& b) {
    if (a.is_null()) return false;
    if (b.is_null()) return true;
    const auto cmp = compare_value_literal(a, to_literal(b));
    if (!cmp) throw sql_error("incomparable types in ORDER BY");
    return *cmp < 0;
}

[[nodiscard]] inline std::string to_display_string(const literal& value) {
    return std::visit([](auto&& v) -> std::string {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, std::monostate>) return "NULL";
        else if constexpr (std::is_same_v<T, bool>) return v ? "true" : "false";
        else if constexpr (std::is_same_v<T, std::int64_t>) return std::to_string(v);
        else if constexpr (std::is_same_v<T, double>) return std::to_string(v);
        else return v;
    }, value);
}

} // namespace opheap::module::sql
