#pragma once

#include "opheap/module/sql/column_type.hpp"

#include <string_view>

namespace opheap::module::sql {

[[nodiscard]] inline std::string_view to_string(column_type type) noexcept {
    switch (type) {
        case column_type::integer: return "INTEGER";
        case column_type::real: return "REAL";
        case column_type::text: return "TEXT";
        case column_type::boolean: return "BOOLEAN";
    }
    return "UNKNOWN";
}

} // namespace opheap::module::sql
