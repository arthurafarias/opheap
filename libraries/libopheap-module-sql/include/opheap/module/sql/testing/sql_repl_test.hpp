#pragma once
#include <opheap/testing/test_group.hpp>
#include <opheap/testing/test_support.hpp>

#include <opheap/module/sql/opheap_sql.hpp>

#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace opheap::testing {

namespace {

int run_repl(std::vector<std::string_view> arguments, std::string input, std::string& output) {
    std::istringstream in{std::move(input)};
    std::ostringstream out;
    std::ostringstream err;
    const auto code = opheap::module::sql::execute(arguments, in, out, err);
    output = out.str();
    return code;
}

} // namespace

struct sql_repl_test : public test_group {
    sql_repl_test() : test_group("sql_repl", {
    {"create, insert, and select round trip through the REPL", [](test_context& ctx) {
        auto directory = temporary_directory("sql-repl-basic");
        std::string output;
        const auto code = run_repl({directory.string()},
            "CREATE TABLE users (id INTEGER, name TEXT);\n"
            "INSERT INTO users (id, name) VALUES (1, 'Arthur');\n"
            "SELECT id, name FROM users;\n"
            ".exit\n",
            output);
        ctx.equal(code, 0);
        ctx.check(output.find("CREATE TABLE") != std::string::npos);
        ctx.check(output.find("INSERT 1") != std::string::npos);
        ctx.check(output.find("Arthur") != std::string::npos);
        ctx.check(output.find("SELECT 1") != std::string::npos);
    }},
    {".tables lists created tables", [](test_context& ctx) {
        auto directory = temporary_directory("sql-repl-tables");
        std::string output;
        const auto code = run_repl({directory.string()},
            "CREATE TABLE a (x INTEGER);\n"
            "CREATE TABLE b (x INTEGER);\n"
            ".tables\n"
            ".exit\n",
            output);
        ctx.equal(code, 0);
        ctx.check(output.find("a") != std::string::npos);
        ctx.check(output.find("b") != std::string::npos);
    }},
    {"a statement error is printed inline and the session continues", [](test_context& ctx) {
        auto directory = temporary_directory("sql-repl-error");
        std::string output;
        const auto code = run_repl({directory.string()},
            "SELECT * FROM missing;\n"
            "CREATE TABLE t (a INTEGER);\n"
            ".exit\n",
            output);
        ctx.equal(code, 0);
        ctx.check(output.find("Error:") != std::string::npos);
        ctx.check(output.find("CREATE TABLE") != std::string::npos);
    }},
    {"a statement split across multiple input lines is executed once complete", [](test_context& ctx) {
        auto directory = temporary_directory("sql-repl-multiline");
        std::string output;
        const auto code = run_repl({directory.string()},
            "CREATE TABLE t (a INTEGER)\n"
            ";\n"
            ".exit\n",
            output);
        ctx.equal(code, 0);
        ctx.check(output.find("CREATE TABLE") != std::string::npos);
    }},
    {"table data survives restart", [](test_context& ctx) {
        auto directory = temporary_directory("sql-repl-restart");
        std::string output;
        ctx.equal(run_repl({directory.string()},
            "CREATE TABLE t (a INTEGER);\nINSERT INTO t (a) VALUES (7);\n.exit\n", output), 0);

        ctx.equal(run_repl({directory.string()}, "SELECT a FROM t;\n.exit\n", output), 0);
        ctx.check(output.find("7") != std::string::npos);
    }},
    {"missing database directory argument is a usage error", [](test_context& ctx) {
        std::istringstream in{""};
        std::ostringstream out;
        std::ostringstream err;
        const auto code = opheap::module::sql::execute(std::vector<std::string_view>{}, in, out, err);
        ctx.check(code == 2);
        ctx.check(err.str().find("usage:") != std::string::npos);
    }},
    }) {}
};

inline static sql_repl_test sql_repl_test_instance;

} // namespace opheap::testing
