#pragma once

#include "opheap/module/sql/column_type.hpp"

#include <string>

namespace opheap::module::sql {

struct column_definition {
    std::string name;
    column_type type{};
};

} // namespace opheap::module::sql
