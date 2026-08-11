#pragma once

#include "opheap/module/sql/column_definition.hpp"

#include <string>
#include <vector>

namespace opheap::module::sql {

struct create_table_statement {
    std::string table;
    std::vector<column_definition> columns;
};

} // namespace opheap::module::sql
