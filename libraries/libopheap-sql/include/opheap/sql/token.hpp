#pragma once

#include <cstdint>
#include <string>

namespace opheap::sql {

enum class token_kind {
    end,
    identifier,
    integer_literal,
    real_literal,
    string_literal,
    kw_select, kw_from, kw_where, kw_insert, kw_into, kw_values,
    kw_update, kw_set, kw_delete, kw_create, kw_table,
    kw_order, kw_by, kw_asc, kw_desc, kw_limit,
    kw_and, kw_or, kw_not,
    kw_null, kw_true, kw_false,
    kw_integer, kw_real, kw_text, kw_boolean,
    comma, dot, star, semicolon,
    lparen, rparen,
    op_eq, op_neq, op_lt, op_lte, op_gt, op_gte,
};

struct token {
    token_kind kind{token_kind::end};
    std::string text{};
    std::int64_t integer_value{};
    double real_value{};
};

} // namespace opheap::sql
