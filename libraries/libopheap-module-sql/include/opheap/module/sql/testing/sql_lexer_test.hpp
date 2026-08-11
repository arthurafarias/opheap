#pragma once
#include <opheap/testing/test_group.hpp>
#include <opheap/testing/test_support.hpp>

#include <opheap/module/sql/lexer.hpp>
#include <opheap/module/sql/sql_error.hpp>

namespace opheap::testing {

struct sql_lexer_test : public test_group {
    sql_lexer_test() : test_group("sql_lexer", {
    {"tokenizes keywords case-insensitively", [](test_context& ctx) {
        auto tokens = opheap::module::sql::tokenize("select * From t");
        ctx.equal(tokens.size(), std::size_t{5});
        ctx.check(tokens[0].kind == opheap::module::sql::token_kind::kw_select);
        ctx.check(tokens[1].kind == opheap::module::sql::token_kind::star);
        ctx.check(tokens[2].kind == opheap::module::sql::token_kind::kw_from);
        ctx.check(tokens[3].kind == opheap::module::sql::token_kind::identifier);
        ctx.equal(tokens[3].text, std::string{"t"});
        ctx.check(tokens[4].kind == opheap::module::sql::token_kind::end);
    }},
    {"tokenizes integer and real literals", [](test_context& ctx) {
        auto tokens = opheap::module::sql::tokenize("42 3.5");
        ctx.check(tokens[0].kind == opheap::module::sql::token_kind::integer_literal);
        ctx.equal(tokens[0].integer_value, std::int64_t{42});
        ctx.check(tokens[1].kind == opheap::module::sql::token_kind::real_literal);
        ctx.equal(tokens[1].real_value, 3.5);
    }},
    {"tokenizes quoted strings with escaped quotes", [](test_context& ctx) {
        auto tokens = opheap::module::sql::tokenize("'it''s ok'");
        ctx.check(tokens[0].kind == opheap::module::sql::token_kind::string_literal);
        ctx.equal(tokens[0].text, std::string{"it's ok"});
    }},
    {"tokenizes comparison operators", [](test_context& ctx) {
        auto tokens = opheap::module::sql::tokenize("= != <> < <= > >=");
        std::vector<opheap::module::sql::token_kind> kinds;
        for (const auto& t : tokens) kinds.push_back(t.kind);
        ctx.check(kinds[0] == opheap::module::sql::token_kind::op_eq);
        ctx.check(kinds[1] == opheap::module::sql::token_kind::op_neq);
        ctx.check(kinds[2] == opheap::module::sql::token_kind::op_neq);
        ctx.check(kinds[3] == opheap::module::sql::token_kind::op_lt);
        ctx.check(kinds[4] == opheap::module::sql::token_kind::op_lte);
        ctx.check(kinds[5] == opheap::module::sql::token_kind::op_gt);
        ctx.check(kinds[6] == opheap::module::sql::token_kind::op_gte);
    }},
    {"skips line comments", [](test_context& ctx) {
        auto tokens = opheap::module::sql::tokenize("SELECT 1 -- trailing comment\n");
        ctx.check(tokens[0].kind == opheap::module::sql::token_kind::kw_select);
        ctx.check(tokens[1].kind == opheap::module::sql::token_kind::integer_literal);
        ctx.check(tokens[2].kind == opheap::module::sql::token_kind::end);
    }},
    {"rejects unterminated string literals", [](test_context& ctx) {
        ctx.throws<opheap::module::sql::sql_error>([&] { (void)opheap::module::sql::tokenize("'unterminated"); });
    }},
    {"rejects unexpected characters", [](test_context& ctx) {
        ctx.throws<opheap::module::sql::sql_error>([&] { (void)opheap::module::sql::tokenize("SELECT # FROM t"); });
    }},
    }) {}
};

inline static sql_lexer_test sql_lexer_test_instance;

} // namespace opheap::testing
