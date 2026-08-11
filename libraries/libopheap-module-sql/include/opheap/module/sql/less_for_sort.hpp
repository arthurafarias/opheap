#pragma once

#include "opheap/module/sql/compare_value_literal.hpp"
#include "opheap/module/sql/sql_error.hpp"
#include "opheap/module/sql/to_literal.hpp"

#include <opheap/value.hpp>

namespace opheap::module::sql {

// NULLs sort after every non-NULL value, regardless of sort direction.
[[nodiscard]] inline bool less_for_sort(const opheap::value& a, const opheap::value& b) {
    if (a.is_null()) return false;
    if (b.is_null()) return true;
    const auto cmp = compare_value_literal(a, to_literal(b));
    if (!cmp) throw sql_error("incomparable types in ORDER BY");
    return *cmp < 0;
}

} // namespace opheap::module::sql
