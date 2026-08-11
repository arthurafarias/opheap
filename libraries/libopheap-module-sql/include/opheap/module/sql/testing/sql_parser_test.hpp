#pragma once
#include <opheap/testing/test_group.hpp>
#include <opheap/testing/test_support.hpp>

#include <opheap/module/sql/parse_statement.hpp>
#include <opheap/module/sql/sql_error.hpp>

namespace opheap::testing {

struct sql_parser_test : public test_group {
    sql_parser_test() : test_group("sql_parser", {
    {"parses CREATE TABLE column list", [](test_context& ctx) {
        auto stmt = std::get<opheap::module::sql::create_table_statement>(
            opheap::module::sql::parse_statement("CREATE TABLE t (a INTEGER, b TEXT, c BOOL);"));
        ctx.equal(stmt.table, std::string{"t"});
        ctx.equal(stmt.columns.size(), std::size_t{3});
        ctx.equal(stmt.columns[0].name, std::string{"a"});
        ctx.check(stmt.columns[0].type == opheap::module::sql::column_type::integer);
        ctx.check(stmt.columns[2].type == opheap::module::sql::column_type::boolean);
    }},
    {"parses INSERT with explicit columns and multiple tuples", [](test_context& ctx) {
        auto stmt = std::get<opheap::module::sql::insert_statement>(
            opheap::module::sql::parse_statement("INSERT INTO t (a, b) VALUES (1, 'x'), (2, 'y')"));
        ctx.equal(stmt.table, std::string{"t"});
        ctx.equal(stmt.columns.size(), std::size_t{2});
        ctx.equal(stmt.rows.size(), std::size_t{2});
        ctx.check(std::get<std::int64_t>(stmt.rows[0][0]) == 1);
        ctx.check(std::get<std::string>(stmt.rows[1][1]) == "y");
    }},
    {"parses SELECT star with WHERE/ORDER BY/LIMIT", [](test_context& ctx) {
        auto stmt = std::get<opheap::module::sql::select_statement>(
            opheap::module::sql::parse_statement("SELECT * FROM t WHERE a = 1 ORDER BY a DESC LIMIT 5"));
        ctx.check(stmt.columns.empty());
        ctx.check(stmt.where != nullptr);
        ctx.check(stmt.order_by_column.has_value());
        ctx.equal(*stmt.order_by_column, std::string{"a"});
        ctx.check(stmt.order_direction == opheap::module::sql::sort_direction::descending);
        ctx.check(stmt.limit.has_value());
        ctx.equal(*stmt.limit, std::int64_t{5});
    }},
    {"AND binds tighter than OR", [](test_context& ctx) {
        auto stmt = std::get<opheap::module::sql::select_statement>(
            opheap::module::sql::parse_statement("SELECT * FROM t WHERE a = 1 OR b = 2 AND c = 3"));
        const auto& root = *stmt.where;
        ctx.check(root.kind == opheap::module::sql::expression_kind::logical);
        ctx.check(root.logic == opheap::module::sql::logical_operator::or_op);
        ctx.check(root.left->kind == opheap::module::sql::expression_kind::comparison);
        ctx.check(root.right->kind == opheap::module::sql::expression_kind::logical);
        ctx.check(root.right->logic == opheap::module::sql::logical_operator::and_op);
    }},
    {"parses NOT and parenthesized groups", [](test_context& ctx) {
        auto stmt = std::get<opheap::module::sql::select_statement>(
            opheap::module::sql::parse_statement("SELECT * FROM t WHERE NOT (a = 1 AND b = 2)"));
        const auto& root = *stmt.where;
        ctx.check(root.kind == opheap::module::sql::expression_kind::negation);
        ctx.check(root.operand->kind == opheap::module::sql::expression_kind::logical);
    }},
    {"parses UPDATE assignments and DELETE with WHERE", [](test_context& ctx) {
        auto update = std::get<opheap::module::sql::update_statement>(
            opheap::module::sql::parse_statement("UPDATE t SET a = 1, b = 'x' WHERE a = 2"));
        ctx.equal(update.assignments.size(), std::size_t{2});
        ctx.check(update.where != nullptr);

        auto del = std::get<opheap::module::sql::delete_statement>(
            opheap::module::sql::parse_statement("DELETE FROM t WHERE a = 1"));
        ctx.equal(del.table, std::string{"t"});
        ctx.check(del.where != nullptr);
    }},
    {"rejects malformed statements", [](test_context& ctx) {
        ctx.throws<opheap::module::sql::sql_error>([] { (void)opheap::module::sql::parse_statement(""); });
        ctx.throws<opheap::module::sql::sql_error>([] { (void)opheap::module::sql::parse_statement("SELECT * FROM"); });
        ctx.throws<opheap::module::sql::sql_error>([] { (void)opheap::module::sql::parse_statement("SELECT * FROM t garbage"); });
        ctx.throws<opheap::module::sql::sql_error>([] { (void)opheap::module::sql::parse_statement("CREATE TABLE t ()"); });
    }},
    }) {}
};

inline static sql_parser_test sql_parser_test_instance;

} // namespace opheap::testing
