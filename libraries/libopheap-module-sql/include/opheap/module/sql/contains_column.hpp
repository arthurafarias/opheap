#pragma once

#include "opheap/module/sql/column_definition.hpp"

#include <algorithm>
#include <string_view>
#include <vector>

namespace opheap::module::sql {

[[nodiscard]] inline bool contains_column(const std::vector<column_definition>& columns, std::string_view name) {
    return std::any_of(columns.begin(), columns.end(), [&](const auto& c) { return c.name == name; });
}

} // namespace opheap::module::sql
