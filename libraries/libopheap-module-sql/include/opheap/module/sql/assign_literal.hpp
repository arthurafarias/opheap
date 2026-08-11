#pragma once

#include "opheap/module/sql/literal.hpp"

#include <opheap/null_t.hpp>
#include <opheap/value.hpp>

#include <type_traits>
#include <variant>

namespace opheap::module::sql {

inline void assign_literal(opheap::value& target, const literal& lit) {
    std::visit([&](auto&& v) {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, std::monostate>) target = opheap::null;
        else target = v;
    }, lit);
}

} // namespace opheap::module::sql
