#pragma once

#include "opheap/module/sql/comparison_operator.hpp"
#include "opheap/module/sql/expression_kind.hpp"
#include "opheap/module/sql/literal.hpp"
#include "opheap/module/sql/logical_operator.hpp"

#include <memory>
#include <string>

namespace opheap::module::sql {

struct expression;
using expression_ptr = std::unique_ptr<expression>;

struct expression {
    expression_kind kind{expression_kind::comparison};

    // comparison: column <op> value
    std::string column;
    comparison_operator comparator{comparison_operator::eq};
    literal value;

    // logical: left <and/or> right
    logical_operator logic{logical_operator::and_op};
    expression_ptr left;
    expression_ptr right;

    // negation: NOT operand
    expression_ptr operand;
};

} // namespace opheap::module::sql
