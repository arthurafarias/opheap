#pragma once

#include "opheap/module/sql/expression.hpp"
#include "opheap/module/sql/literal.hpp"

#include <string>
#include <utility>
#include <vector>

namespace opheap::module::sql {

struct update_statement {
    std::string table;
    std::vector<std::pair<std::string, literal>> assignments;
    expression_ptr where;
};

} // namespace opheap::module::sql
