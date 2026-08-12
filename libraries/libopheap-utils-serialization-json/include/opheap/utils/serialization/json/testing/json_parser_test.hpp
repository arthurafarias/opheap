#pragma once
#include <opheap/testing/test_group.hpp>
#include <opheap/testing/test_support.hpp>

#include <opheap/utils/serialization/json/json_error.hpp>
#include <opheap/utils/serialization/json/json_parser.hpp>

#include <opheap/value.hpp>

#include <memory_resource>
#include <string_view>

namespace opheap::testing {

namespace {

opheap::value parse_json(std::string_view text) {
    return opheap::utils::serialization::json::json_parser{text, std::pmr::get_default_resource()}.parse();
}

void expect_json_error(test_context& ctx, std::string_view text) {
    ctx.throws<opheap::utils::serialization::json::json_error>([&] { (void)parse_json(text); },
        "expected json_error for input");
}

} // namespace

struct json_parser_test : public test_group {
    json_parser_test() : test_group("json_parser", {
    {"parses every scalar and container shape", [](test_context& ctx) {
        ctx.check(parse_json("null").is_null());
        ctx.check(parse_json("true").as_bool());
        ctx.check(!parse_json("false").as_bool());
        ctx.equal(parse_json("42").as_integer(), std::int64_t{42});
        ctx.equal(parse_json("-3.5").as_number(), -3.5);
        ctx.equal(parse_json("1e2").as_number(), 100.0);
        ctx.equal(parse_json(R"("hi")").as_string().view(), std::string_view{"hi"});
        ctx.equal(parse_json("[1,2,3]").as_array().size(), std::size_t{3});
        ctx.equal(parse_json(R"({"a":1,"b":2})").as_object().size(), std::size_t{2});
    }},
    {"decodes string escapes including surrogate pairs", [](test_context& ctx) {
        ctx.equal(parse_json(R"("a\nb")").as_string().view(), std::string_view{"a\nb"});
        ctx.equal(parse_json(R"("\u0041")").as_string().view(), std::string_view{"A"});
        // U+1F600 GRINNING FACE, encoded as a UTF-16 surrogate pair and decoded to UTF-8.
        ctx.equal(parse_json(R"("\ud83d\ude00")").as_string().view(), std::string_view{"\xf0\x9f\x98\x80"});
    }},
    {"rejects empty input and trailing input", [](test_context& ctx) {
        expect_json_error(ctx, "");
        expect_json_error(ctx, "   ");
        expect_json_error(ctx, "1 2");
        expect_json_error(ctx, "[1] garbage");
    }},
    {"rejects a malformed literal keyword", [](test_context& ctx) {
        expect_json_error(ctx, "nul");
        expect_json_error(ctx, "truX");
        expect_json_error(ctx, "fals");
    }},
    {"rejects malformed string escapes", [](test_context& ctx) {
        expect_json_error(ctx, "\"abc\\");           // backslash at end of input: incomplete escape
        expect_json_error(ctx, "\"\\q\"");            // unrecognized escape character
        expect_json_error(ctx, "\"abc");              // unterminated string
        expect_json_error(ctx, "\"a\tb\"");           // unescaped raw control character
    }},
    {"rejects malformed Unicode escapes and surrogate pairs", [](test_context& ctx) {
        expect_json_error(ctx, "\"\\u12\"");           // too few hex digits
        expect_json_error(ctx, "\"\\uZZZZ\"");         // non-hex digits
        expect_json_error(ctx, "\"\\ud800\"");         // high surrogate with nothing following
        expect_json_error(ctx, "\"\\ud800\\u0041\"");  // high surrogate followed by a non-surrogate escape
        expect_json_error(ctx, "\"\\udc00\"");         // lone low surrogate
    }},
    {"rejects malformed arrays and objects", [](test_context& ctx) {
        expect_json_error(ctx, "[1 2]");
        expect_json_error(ctx, "{1:2}");
        expect_json_error(ctx, R"({"a" 1})");
        expect_json_error(ctx, R"({"a":1 "b":2})");
        expect_json_error(ctx, R"({"a":1,"a":2})");
    }},
    {"rejects malformed numbers", [](test_context& ctx) {
        expect_json_error(ctx, "q");         // not a recognized value at all
        expect_json_error(ctx, "-");         // sign with no digits
        expect_json_error(ctx, "01");        // leading zero
        expect_json_error(ctx, "1.");        // fraction with no digits
        expect_json_error(ctx, "1e");        // exponent with no digits
        expect_json_error(ctx, "1e999");     // exponent overflows to infinity
    }},
    }) {}
};

inline static json_parser_test json_parser_test_instance;

} // namespace opheap::testing
