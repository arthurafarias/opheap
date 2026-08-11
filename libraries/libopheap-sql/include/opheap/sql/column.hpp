#pragma once

#include "opheap/sql/sql_error.hpp"

#include <string>
#include <string_view>

namespace opheap::sql {

enum class column_type { integer, real, text, boolean };

[[nodiscard]] inline std::string_view to_string(column_type type) noexcept {
    switch (type) {
        case column_type::integer: return "INTEGER";
        case column_type::real: return "REAL";
        case column_type::text: return "TEXT";
        case column_type::boolean: return "BOOLEAN";
    }
    return "UNKNOWN";
}

[[nodiscard]] inline column_type column_type_from_string(std::string_view text) {
    if (text == "INTEGER") return column_type::integer;
    if (text == "REAL") return column_type::real;
    if (text == "TEXT") return column_type::text;
    if (text == "BOOLEAN") return column_type::boolean;
    throw sql_error("unknown column type '" + std::string{text} + "' in catalog");
}

struct column_definition {
    std::string name;
    column_type type{};
};

} // namespace opheap::sql
