#pragma once
#include "test_group.hpp"
#include "test_support.hpp"

#include <opheap/heap.hpp>
#include <opheap/sql/interpreter.hpp>
#include <opheap/sql/sql_error.hpp>

namespace opheap::testing {

inline static test_group sql_interpreter_test{"sql_interpreter", {
    {"create/insert/select round trip", [](test_context& ctx) {
        auto directory = temporary_directory("sql-basic");
        auto heap = opheap::heap::open({.path = directory});
        opheap::sql::interpreter sql{heap};

        sql.execute("CREATE TABLE users (id INTEGER, name TEXT, active BOOLEAN, score REAL)");
        sql.execute("INSERT INTO users (id, name, active, score) VALUES (1, 'Arthur', true, 9.5), "
                    "(2, 'Bea', false, 3.25)");

        auto res = sql.execute("SELECT id, name FROM users ORDER BY id");
        ctx.equal(res.columns.size(), std::size_t{2});
        ctx.equal(res.rows.size(), std::size_t{2});
        ctx.equal(std::get<std::int64_t>(res.rows[0][0]), std::int64_t{1});
        ctx.equal(std::get<std::string>(res.rows[0][1]), std::string{"Arthur"});
        ctx.equal(std::get<std::int64_t>(res.rows[1][0]), std::int64_t{2});
    }},
    {"INSERT coerces integer literals into REAL columns", [](test_context& ctx) {
        auto directory = temporary_directory("sql-coerce");
        auto heap = opheap::heap::open({.path = directory});
        opheap::sql::interpreter sql{heap};
        sql.execute("CREATE TABLE t (a REAL)");
        sql.execute("INSERT INTO t (a) VALUES (5)");
        auto res = sql.execute("SELECT a FROM t");
        ctx.equal(std::get<double>(res.rows[0][0]), 5.0);
    }},
    {"unspecified columns default to NULL", [](test_context& ctx) {
        auto directory = temporary_directory("sql-null-default");
        auto heap = opheap::heap::open({.path = directory});
        opheap::sql::interpreter sql{heap};
        sql.execute("CREATE TABLE t (a INTEGER, b TEXT)");
        sql.execute("INSERT INTO t (a) VALUES (1)");
        auto res = sql.execute("SELECT b FROM t");
        ctx.check(std::holds_alternative<std::monostate>(res.rows[0][0]));
    }},
    {"WHERE filters with AND/OR/NOT and comparisons", [](test_context& ctx) {
        auto directory = temporary_directory("sql-where");
        auto heap = opheap::heap::open({.path = directory});
        opheap::sql::interpreter sql{heap};
        sql.execute("CREATE TABLE t (a INTEGER, b TEXT)");
        sql.execute("INSERT INTO t (a, b) VALUES (1, 'x'), (2, 'y'), (3, 'x')");

        auto res = sql.execute("SELECT a FROM t WHERE b = 'x' AND a > 1");
        ctx.equal(res.rows.size(), std::size_t{1});
        ctx.equal(std::get<std::int64_t>(res.rows[0][0]), std::int64_t{3});

        auto res2 = sql.execute("SELECT a FROM t WHERE NOT (b = 'x')");
        ctx.equal(res2.rows.size(), std::size_t{1});
        ctx.equal(std::get<std::int64_t>(res2.rows[0][0]), std::int64_t{2});
    }},
    {"ORDER BY DESC and LIMIT", [](test_context& ctx) {
        auto directory = temporary_directory("sql-order-limit");
        auto heap = opheap::heap::open({.path = directory});
        opheap::sql::interpreter sql{heap};
        sql.execute("CREATE TABLE t (a INTEGER)");
        sql.execute("INSERT INTO t (a) VALUES (3), (1), (2)");
        auto res = sql.execute("SELECT a FROM t ORDER BY a DESC LIMIT 2");
        ctx.equal(res.rows.size(), std::size_t{2});
        ctx.equal(std::get<std::int64_t>(res.rows[0][0]), std::int64_t{3});
        ctx.equal(std::get<std::int64_t>(res.rows[1][0]), std::int64_t{2});
    }},
    {"UPDATE mutates matching rows only", [](test_context& ctx) {
        auto directory = temporary_directory("sql-update");
        auto heap = opheap::heap::open({.path = directory});
        opheap::sql::interpreter sql{heap};
        sql.execute("CREATE TABLE t (a INTEGER, b TEXT)");
        sql.execute("INSERT INTO t (a, b) VALUES (1, 'x'), (2, 'x')");
        auto update = sql.execute("UPDATE t SET b = 'y' WHERE a = 1");
        ctx.equal(update.rows_affected, std::size_t{1});
        auto res = sql.execute("SELECT a, b FROM t ORDER BY a");
        ctx.equal(std::get<std::string>(res.rows[0][1]), std::string{"y"});
        ctx.equal(std::get<std::string>(res.rows[1][1]), std::string{"x"});
    }},
    {"DELETE removes matching rows only", [](test_context& ctx) {
        auto directory = temporary_directory("sql-delete");
        auto heap = opheap::heap::open({.path = directory});
        opheap::sql::interpreter sql{heap};
        sql.execute("CREATE TABLE t (a INTEGER)");
        sql.execute("INSERT INTO t (a) VALUES (1), (2), (3)");
        auto deleted = sql.execute("DELETE FROM t WHERE a = 2");
        ctx.equal(deleted.rows_affected, std::size_t{1});
        auto res = sql.execute("SELECT a FROM t ORDER BY a");
        ctx.equal(res.rows.size(), std::size_t{2});
        ctx.equal(std::get<std::int64_t>(res.rows[0][0]), std::int64_t{1});
        ctx.equal(std::get<std::int64_t>(res.rows[1][0]), std::int64_t{3});
    }},
    {"table data survives restart", [](test_context& ctx) {
        auto directory = temporary_directory("sql-restart");
        {
            auto heap = opheap::heap::open({.path = directory});
            opheap::sql::interpreter sql{heap};
            sql.execute("CREATE TABLE t (a INTEGER)");
            sql.execute("INSERT INTO t (a) VALUES (7)");
        }
        {
            auto heap = opheap::heap::open({.path = directory});
            opheap::sql::interpreter sql{heap};
            auto res = sql.execute("SELECT a FROM t");
            ctx.equal(res.rows.size(), std::size_t{1});
            ctx.equal(std::get<std::int64_t>(res.rows[0][0]), std::int64_t{7});
        }
    }},
    {"list_tables reports created tables", [](test_context& ctx) {
        auto directory = temporary_directory("sql-list-tables");
        auto heap = opheap::heap::open({.path = directory});
        opheap::sql::interpreter sql{heap};
        sql.execute("CREATE TABLE a (x INTEGER)");
        sql.execute("CREATE TABLE b (x INTEGER)");
        auto tables = sql.list_tables();
        ctx.equal(tables.rows.size(), std::size_t{2});
    }},
    {"rejects operations against unknown tables and columns", [](test_context& ctx) {
        auto directory = temporary_directory("sql-errors");
        auto heap = opheap::heap::open({.path = directory});
        opheap::sql::interpreter sql{heap};
        sql.execute("CREATE TABLE t (a INTEGER)");

        ctx.throws<opheap::sql::sql_error>([&] { sql.execute("SELECT * FROM missing"); });
        ctx.throws<opheap::sql::sql_error>([&] { sql.execute("SELECT missing_column FROM t"); });
        ctx.throws<opheap::sql::sql_error>([&] { sql.execute("CREATE TABLE t (a INTEGER)"); });
        ctx.throws<opheap::sql::sql_error>([&] { sql.execute("INSERT INTO t (a) VALUES ('not an int')"); });
    }},
}};

} // namespace opheap::testing
