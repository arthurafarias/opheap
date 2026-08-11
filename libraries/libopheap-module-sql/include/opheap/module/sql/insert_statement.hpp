#pragma once

#include "opheap/module/sql/literal.hpp"

#include <string>
#include <vector>

namespace opheap::module::sql {

struct insert_statement {
    std::string table;
    std::vector<std::string> columns; // empty => catalog order
    std::vector<std::vector<literal>> rows;
};

} // namespace opheap::module::sql
