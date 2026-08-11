#pragma once

#include "opheap/module/sql/expression.hpp"
#include "opheap/module/sql/sort_direction.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace opheap::module::sql {

struct select_statement {
    std::string table;
    std::vector<std::string> columns; // empty => '*'
    expression_ptr where;
    std::optional<std::string> order_by_column;
    sort_direction order_direction{sort_direction::ascending};
    std::optional<std::int64_t> limit;
};

} // namespace opheap::module::sql
