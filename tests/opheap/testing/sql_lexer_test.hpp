#pragma once
#include "test_group.hpp"
#include "test_support.hpp"

#include <opheap/sql/lexer.hpp>
#include <opheap/sql/sql_error.hpp>

namespace opheap::testing {

inline static test_group sql_lexer_test{"sql_lexer", {
    {"tokenizes keywords case-insensitively", [](test_context& ctx) {
        auto tokens = opheap::sql::tokenize("select * From t");
        ctx.equal(tokens.size(), std::size_t{5});
        ctx.check(tokens[0].kind == opheap::sql::token_kind::kw_select);
        ctx.check(tokens[1].kind == opheap::sql::token_kind::star);
        ctx.check(tokens[2].kind == opheap::sql::token_kind::kw_from);
        ctx.check(tokens[3].kind == opheap::sql::token_kind::identifier);
        ctx.equal(tokens[3].text, std::string{"t"});
        ctx.check(tokens[4].kind == opheap::sql::token_kind::end);
    }},
    {"tokenizes integer and real literals", [](test_context& ctx) {
        auto tokens = opheap::sql::tokenize("42 3.5");
        ctx.check(tokens[0].kind == opheap::sql::token_kind::integer_literal);
        ctx.equal(tokens[0].integer_value, std::int64_t{42});
        ctx.check(tokens[1].kind == opheap::sql::token_kind::real_literal);
        ctx.equal(tokens[1].real_value, 3.5);
    }},
    {"tokenizes quoted strings with escaped quotes", [](test_context& ctx) {
        auto tokens = opheap::sql::tokenize("'it''s ok'");
        ctx.check(tokens[0].kind == opheap::sql::token_kind::string_literal);
        ctx.equal(tokens[0].text, std::string{"it's ok"});
    }},
    {"tokenizes comparison operators", [](test_context& ctx) {
        auto tokens = opheap::sql::tokenize("= != <> < <= > >=");
        std::vector<opheap::sql::token_kind> kinds;
        for (const auto& t : tokens) kinds.push_back(t.kind);
        ctx.check(kinds[0] == opheap::sql::token_kind::op_eq);
        ctx.check(kinds[1] == opheap::sql::token_kind::op_neq);
        ctx.check(kinds[2] == opheap::sql::token_kind::op_neq);
        ctx.check(kinds[3] == opheap::sql::token_kind::op_lt);
        ctx.check(kinds[4] == opheap::sql::token_kind::op_lte);
        ctx.check(kinds[5] == opheap::sql::token_kind::op_gt);
        ctx.check(kinds[6] == opheap::sql::token_kind::op_gte);
    }},
    {"skips line comments", [](test_context& ctx) {
        auto tokens = opheap::sql::tokenize("SELECT 1 -- trailing comment\n");
        ctx.check(tokens[0].kind == opheap::sql::token_kind::kw_select);
        ctx.check(tokens[1].kind == opheap::sql::token_kind::integer_literal);
        ctx.check(tokens[2].kind == opheap::sql::token_kind::end);
    }},
    {"rejects unterminated string literals", [](test_context& ctx) {
        ctx.throws<opheap::sql::sql_error>([&] { (void)opheap::sql::tokenize("'unterminated"); });
    }},
    {"rejects unexpected characters", [](test_context& ctx) {
        ctx.throws<opheap::sql::sql_error>([&] { (void)opheap::sql::tokenize("SELECT # FROM t"); });
    }},
}};

} // namespace opheap::testing
