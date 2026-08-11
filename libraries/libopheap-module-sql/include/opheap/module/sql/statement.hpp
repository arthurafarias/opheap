#pragma once

#include "opheap/module/sql/create_table_statement.hpp"
#include "opheap/module/sql/delete_statement.hpp"
#include "opheap/module/sql/insert_statement.hpp"
#include "opheap/module/sql/select_statement.hpp"
#include "opheap/module/sql/update_statement.hpp"

#include <variant>

namespace opheap::module::sql {

using statement = std::variant<create_table_statement, insert_statement, select_statement,
                                update_statement, delete_statement>;

} // namespace opheap::module::sql
