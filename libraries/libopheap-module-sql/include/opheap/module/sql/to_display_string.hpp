#pragma once

#include "opheap/module/sql/literal.hpp"

#include <cstdint>
#include <string>
#include <type_traits>
#include <variant>

namespace opheap::module::sql {

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
