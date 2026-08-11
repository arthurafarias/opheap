#pragma once

#include "opheap/module/sql/column_definition.hpp"
#include "opheap/module/sql/column_type.hpp"
#include "opheap/module/sql/comparison_operator.hpp"
#include "opheap/module/sql/create_table_statement.hpp"
#include "opheap/module/sql/delete_statement.hpp"
#include "opheap/module/sql/expression.hpp"
#include "opheap/module/sql/insert_statement.hpp"
#include "opheap/module/sql/literal.hpp"
#include "opheap/module/sql/logical_operator.hpp"
#include "opheap/module/sql/make_comparison.hpp"
#include "opheap/module/sql/make_logical.hpp"
#include "opheap/module/sql/make_negation.hpp"
#include "opheap/module/sql/select_statement.hpp"
#include "opheap/module/sql/sort_direction.hpp"
#include "opheap/module/sql/sql_error.hpp"
#include "opheap/module/sql/statement.hpp"
#include "opheap/module/sql/token.hpp"
#include "opheap/module/sql/tokenize.hpp"
#include "opheap/module/sql/update_statement.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace opheap::module::sql {

namespace detail {

struct token_stream {
    std::vector<token> tokens;
    std::size_t position{0};

    [[nodiscard]] const token& peek() const noexcept { return tokens[position]; }

    const token& advance() {
        const auto& current = tokens[position];
        if (position + 1 < tokens.size()) ++position;
        return current;
    }

    [[nodiscard]] bool check(token_kind kind) const noexcept { return peek().kind == kind; }

    bool match(token_kind kind) {
        if (!check(kind)) return false;
        advance();
        return true;
    }

    const token& expect(token_kind kind, std::string_view what) {
        if (!check(kind)) throw sql_error("expected " + std::string{what});
        return advance();
    }
};

inline column_type parse_column_type(token_stream& ts) {
    const auto& t = ts.advance();
    switch (t.kind) {
        case token_kind::kw_integer: return column_type::integer;
        case token_kind::kw_real: return column_type::real;
        case token_kind::kw_text: return column_type::text;
        case token_kind::kw_boolean: return column_type::boolean;
        default: throw sql_error("expected column type");
    }
}

inline literal parse_literal(token_stream& ts) {
    const auto& t = ts.advance();
    switch (t.kind) {
        case token_kind::integer_literal: return literal{t.integer_value};
        case token_kind::real_literal: return literal{t.real_value};
        case token_kind::string_literal: return literal{t.text};
        case token_kind::kw_true: return literal{true};
        case token_kind::kw_false: return literal{false};
        case token_kind::kw_null: return literal{std::monostate{}};
        default: throw sql_error("expected a literal value");
    }
}

inline comparison_operator parse_comparator(token_stream& ts) {
    const auto& t = ts.advance();
    switch (t.kind) {
        case token_kind::op_eq: return comparison_operator::eq;
        case token_kind::op_neq: return comparison_operator::neq;
        case token_kind::op_lt: return comparison_operator::lt;
        case token_kind::op_lte: return comparison_operator::lte;
        case token_kind::op_gt: return comparison_operator::gt;
        case token_kind::op_gte: return comparison_operator::gte;
        default: throw sql_error("expected a comparison operator");
    }
}

inline expression_ptr parse_expression(token_stream& ts);

inline expression_ptr parse_unary_expression(token_stream& ts) {
    if (ts.match(token_kind::kw_not)) return make_negation(parse_unary_expression(ts));
    if (ts.match(token_kind::lparen)) {
        auto expr = parse_expression(ts);
        ts.expect(token_kind::rparen, "')'");
        return expr;
    }
    auto column = ts.expect(token_kind::identifier, "a column name").text;
    auto op = parse_comparator(ts);
    auto value = parse_literal(ts);
    return make_comparison(std::move(column), op, std::move(value));
}

inline expression_ptr parse_and_expression(token_stream& ts) {
    auto left = parse_unary_expression(ts);
    while (ts.match(token_kind::kw_and)) {
        auto right = parse_unary_expression(ts);
        left = make_logical(logical_operator::and_op, std::move(left), std::move(right));
    }
    return left;
}

inline expression_ptr parse_expression(token_stream& ts) {
    auto left = parse_and_expression(ts);
    while (ts.match(token_kind::kw_or)) {
        auto right = parse_and_expression(ts);
        left = make_logical(logical_operator::or_op, std::move(left), std::move(right));
    }
    return left;
}

inline std::vector<std::string> parse_identifier_list(token_stream& ts) {
    std::vector<std::string> names;
    names.push_back(ts.expect(token_kind::identifier, "an identifier").text);
    while (ts.match(token_kind::comma)) names.push_back(ts.expect(token_kind::identifier, "an identifier").text);
    return names;
}

inline create_table_statement parse_create_table(token_stream& ts) {
    ts.expect(token_kind::kw_create, "CREATE");
    ts.expect(token_kind::kw_table, "TABLE");
    create_table_statement stmt;
    stmt.table = ts.expect(token_kind::identifier, "a table name").text;
    ts.expect(token_kind::lparen, "'('");
    do {
        column_definition column;
        column.name = ts.expect(token_kind::identifier, "a column name").text;
        column.type = parse_column_type(ts);
        stmt.columns.push_back(std::move(column));
    } while (ts.match(token_kind::comma));
    ts.expect(token_kind::rparen, "')'");
    return stmt;
}

inline std::vector<literal> parse_value_tuple(token_stream& ts) {
    ts.expect(token_kind::lparen, "'('");
    std::vector<literal> values;
    values.push_back(parse_literal(ts));
    while (ts.match(token_kind::comma)) values.push_back(parse_literal(ts));
    ts.expect(token_kind::rparen, "')'");
    return values;
}

inline insert_statement parse_insert(token_stream& ts) {
    ts.expect(token_kind::kw_insert, "INSERT");
    ts.expect(token_kind::kw_into, "INTO");
    insert_statement stmt;
    stmt.table = ts.expect(token_kind::identifier, "a table name").text;
    if (ts.match(token_kind::lparen)) {
        stmt.columns.push_back(ts.expect(token_kind::identifier, "a column name").text);
        while (ts.match(token_kind::comma)) stmt.columns.push_back(ts.expect(token_kind::identifier, "a column name").text);
        ts.expect(token_kind::rparen, "')'");
    }
    ts.expect(token_kind::kw_values, "VALUES");
    stmt.rows.push_back(parse_value_tuple(ts));
    while (ts.match(token_kind::comma)) stmt.rows.push_back(parse_value_tuple(ts));
    return stmt;
}

inline select_statement parse_select(token_stream& ts) {
    ts.expect(token_kind::kw_select, "SELECT");
    select_statement stmt;
    if (!ts.match(token_kind::star)) stmt.columns = parse_identifier_list(ts);
    ts.expect(token_kind::kw_from, "FROM");
    stmt.table = ts.expect(token_kind::identifier, "a table name").text;
    if (ts.match(token_kind::kw_where)) stmt.where = parse_expression(ts);
    if (ts.match(token_kind::kw_order)) {
        ts.expect(token_kind::kw_by, "BY");
        stmt.order_by_column = ts.expect(token_kind::identifier, "a column name").text;
        if (ts.match(token_kind::kw_desc)) stmt.order_direction = sort_direction::descending;
        else ts.match(token_kind::kw_asc);
    }
    if (ts.match(token_kind::kw_limit)) {
        stmt.limit = ts.expect(token_kind::integer_literal, "an integer").integer_value;
    }
    return stmt;
}

inline update_statement parse_update(token_stream& ts) {
    ts.expect(token_kind::kw_update, "UPDATE");
    update_statement stmt;
    stmt.table = ts.expect(token_kind::identifier, "a table name").text;
    ts.expect(token_kind::kw_set, "SET");
    do {
        auto name = ts.expect(token_kind::identifier, "a column name").text;
        ts.expect(token_kind::op_eq, "'='");
        auto value = parse_literal(ts);
        stmt.assignments.emplace_back(std::move(name), std::move(value));
    } while (ts.match(token_kind::comma));
    if (ts.match(token_kind::kw_where)) stmt.where = parse_expression(ts);
    return stmt;
}

inline delete_statement parse_delete(token_stream& ts) {
    ts.expect(token_kind::kw_delete, "DELETE");
    ts.expect(token_kind::kw_from, "FROM");
    delete_statement stmt;
    stmt.table = ts.expect(token_kind::identifier, "a table name").text;
    if (ts.match(token_kind::kw_where)) stmt.where = parse_expression(ts);
    return stmt;
}

} // namespace detail

inline statement parse_statement(std::string_view text) {
    detail::token_stream ts{tokenize(text)};
    if (ts.check(token_kind::end)) throw sql_error("empty statement");

    statement result = [&]() -> statement {
        switch (ts.peek().kind) {
            case token_kind::kw_create: return detail::parse_create_table(ts);
            case token_kind::kw_insert: return detail::parse_insert(ts);
            case token_kind::kw_select: return detail::parse_select(ts);
            case token_kind::kw_update: return detail::parse_update(ts);
            case token_kind::kw_delete: return detail::parse_delete(ts);
            default: throw sql_error("unrecognized statement");
        }
    }();

    ts.match(token_kind::semicolon);
    if (!ts.check(token_kind::end)) throw sql_error("unexpected trailing input");
    return result;
}

} // namespace opheap::module::sql
