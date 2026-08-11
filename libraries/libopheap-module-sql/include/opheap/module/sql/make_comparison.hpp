#pragma once

#include "opheap/module/sql/comparison_operator.hpp"
#include "opheap/module/sql/expression.hpp"
#include "opheap/module/sql/literal.hpp"

#include <memory>
#include <string>
#include <utility>

namespace opheap::module::sql {

inline expression_ptr make_comparison(std::string column, comparison_operator op, literal value) {
    auto node = std::make_unique<expression>();
    node->kind = expression_kind::comparison;
    node->column = std::move(column);
    node->comparator = op;
    node->value = std::move(value);
    return node;
}

} // namespace opheap::module::sql
