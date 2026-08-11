#pragma once

#include "opheap/module/sql/expression.hpp"
#include "opheap/module/sql/logical_operator.hpp"

#include <memory>
#include <utility>

namespace opheap::module::sql {

inline expression_ptr make_logical(logical_operator op, expression_ptr left, expression_ptr right) {
    auto node = std::make_unique<expression>();
    node->kind = expression_kind::logical;
    node->logic = op;
    node->left = std::move(left);
    node->right = std::move(right);
    return node;
}

} // namespace opheap::module::sql
