#pragma once

#include "opheap/module/sql/expression.hpp"

#include <memory>
#include <utility>

namespace opheap::module::sql {

inline expression_ptr make_negation(expression_ptr operand) {
    auto node = std::make_unique<expression>();
    node->kind = expression_kind::negation;
    node->operand = std::move(operand);
    return node;
}

} // namespace opheap::module::sql
