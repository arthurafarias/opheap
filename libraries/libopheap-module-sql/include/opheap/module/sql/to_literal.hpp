#pragma once

#include "opheap/module/sql/literal.hpp"
#include "opheap/module/sql/sql_error.hpp"

#include <opheap/value.hpp>

#include <string>

namespace opheap::module::sql {

[[nodiscard]] inline literal to_literal(const opheap::value& v) {
    if (v.is_null()) return literal{std::monostate{}};
    if (v.is_bool()) return literal{v.as_bool()};
    if (v.is_integer()) return literal{v.as_integer()};
    if (v.is_number()) return literal{v.as_number()};
    if (v.is_string()) return literal{std::string{v.as_string().view()}};
    throw sql_error("value has no SQL-representable type");
}

} // namespace opheap::module::sql
