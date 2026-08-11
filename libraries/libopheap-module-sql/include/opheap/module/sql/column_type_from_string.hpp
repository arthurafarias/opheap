#pragma once

#include "opheap/module/sql/column_type.hpp"
#include "opheap/module/sql/sql_error.hpp"

#include <string>
#include <string_view>

namespace opheap::module::sql {

[[nodiscard]] inline column_type column_type_from_string(std::string_view text) {
    if (text == "INTEGER") return column_type::integer;
    if (text == "REAL") return column_type::real;
    if (text == "TEXT") return column_type::text;
    if (text == "BOOLEAN") return column_type::boolean;
    throw sql_error("unknown column type '" + std::string{text} + "' in catalog");
}

} // namespace opheap::module::sql
