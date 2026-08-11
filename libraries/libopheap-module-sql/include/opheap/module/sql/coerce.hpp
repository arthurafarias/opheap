#pragma once

#include "opheap/module/sql/column_type.hpp"
#include "opheap/module/sql/literal.hpp"
#include "opheap/module/sql/sql_error.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <variant>

namespace opheap::module::sql {

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

} // namespace opheap::module::sql
