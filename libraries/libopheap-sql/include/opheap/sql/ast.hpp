#pragma once

#include "opheap/sql/column.hpp"
#include "opheap/sql/literal.hpp"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace opheap::sql {

enum class expression_kind { comparison, logical, negation };
enum class comparison_operator { eq, neq, lt, lte, gt, gte };
enum class logical_operator { and_op, or_op };

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

inline expression_ptr make_comparison(std::string column, comparison_operator op, literal value) {
    auto node = std::make_unique<expression>();
    node->kind = expression_kind::comparison;
    node->column = std::move(column);
    node->comparator = op;
    node->value = std::move(value);
    return node;
}

inline expression_ptr make_logical(logical_operator op, expression_ptr left, expression_ptr right) {
    auto node = std::make_unique<expression>();
    node->kind = expression_kind::logical;
    node->logic = op;
    node->left = std::move(left);
    node->right = std::move(right);
    return node;
}

inline expression_ptr make_negation(expression_ptr operand) {
    auto node = std::make_unique<expression>();
    node->kind = expression_kind::negation;
    node->operand = std::move(operand);
    return node;
}

struct create_table_statement {
    std::string table;
    std::vector<column_definition> columns;
};

struct insert_statement {
    std::string table;
    std::vector<std::string> columns; // empty => catalog order
    std::vector<std::vector<literal>> rows;
};

enum class sort_direction { ascending, descending };

struct select_statement {
    std::string table;
    std::vector<std::string> columns; // empty => '*'
    expression_ptr where;
    std::optional<std::string> order_by_column;
    sort_direction order_direction{sort_direction::ascending};
    std::optional<std::int64_t> limit;
};

struct update_statement {
    std::string table;
    std::vector<std::pair<std::string, literal>> assignments;
    expression_ptr where;
};

struct delete_statement {
    std::string table;
    expression_ptr where;
};

using statement = std::variant<create_table_statement, insert_statement, select_statement,
                                update_statement, delete_statement>;

} // namespace opheap::sql
