#pragma once

#include "opheap/module/sql/expression.hpp"

#include <string>

namespace opheap::module::sql {

struct delete_statement {
    std::string table;
    expression_ptr where;
};

} // namespace opheap::module::sql
