#pragma once
#include <opheap/testing/test_group.hpp>
#include <opheap/testing/test_support.hpp>

#include <opheap/module/cli/opheap_cli.hpp>

#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace opheap::testing {

namespace {

int run_cli(std::vector<std::string_view> arguments, std::string input, std::string& output) {
    std::istringstream in{std::move(input)};
    std::ostringstream out;
    std::ostringstream err;
    const auto code = opheap::module::cli::execute(arguments, in, out, err);
    output = out.str();
    return code;
}

} // namespace

struct cli_test : public test_group {
    cli_test() : test_group("cli", {
    {"round-trips JSON scalars, strings, and nesting", [](test_context& ctx) {
        opheap::value scratch;
        auto parse = [&](std::string_view text) {
            return opheap::utils::serialization::json::json_parser{text, scratch.resource()}.parse();
        };
        auto printed = [&](const opheap::value& value) {
            std::ostringstream out;
            opheap::utils::serialization::json::write_json(out, value);
            return out.str();
        };
        ctx.equal(printed(parse("null")), std::string{"null"});
        ctx.equal(printed(parse("true")), std::string{"true"});
        ctx.equal(printed(parse("42")), std::string{"42"});
        ctx.equal(printed(parse("3.5")), std::string{"3.5"});
        ctx.equal(printed(parse("\"a\\nb\"")), std::string{"\"a\\nb\""});
        ctx.equal(printed(parse(R"({"a":[1,2,"x"]})")), std::string{R"({"a":[1,2,"x"]})"});
    }},
    {"rejects malformed JSON", [](test_context& ctx) {
        opheap::value scratch;
        ctx.throws<opheap::utils::serialization::json::json_error>([&] {
            (void)opheap::utils::serialization::json::json_parser{"{\"a\":}", scratch.resource()}.parse();
        });
        ctx.throws<opheap::utils::serialization::json::json_error>([&] {
            (void)opheap::utils::serialization::json::json_parser{"[1,]", scratch.resource()}.parse();
        });
    }},
    {"create, get, update, and delete a root end to end", [](test_context& ctx) {
        auto directory = temporary_directory("cli-crud");
        std::string output;

        ctx.equal(run_cli({"-C", directory.string(), "create", "doc", R"({"name":"Arthur","age":42})"},
            "", output), 0);
        ctx.equal(output, std::string{R"({"age":42,"name":"Arthur"})" "\n"});

        ctx.equal(run_cli({"-C", directory.string(), "get", "doc.name"}, "", output), 0);
        ctx.equal(output, std::string{"\"Arthur\"\n"});

        ctx.equal(run_cli({"-C", directory.string(), "update", "doc", R"({"name":"Arthur","age":43})"},
            "", output), 0);
        ctx.equal(output, std::string{R"({"age":43,"name":"Arthur"})" "\n"});

        ctx.equal(run_cli({"-C", directory.string(), "delete", "doc"}, "", output), 0);

        std::string error_output;
        std::istringstream in{""};
        std::ostringstream out;
        std::ostringstream err;
        const auto code = opheap::module::cli::execute(
            std::vector<std::string_view>{"-C", directory.string(), "get", "doc"}, in, out, err);
        ctx.check(code == 1);
        ctx.check(err.str().find("root not found") != std::string::npos);
    }},
    {"reads JSON payload from stdin when argument is '-'", [](test_context& ctx) {
        auto directory = temporary_directory("cli-stdin");
        std::string output;
        ctx.equal(run_cli({"-C", directory.string(), "create", "doc", "-"}, R"([1,2,3])", output), 0);
        ctx.equal(output, std::string{"[1,2,3]\n"});
    }},
    {"inspect lists every root as a single JSON object", [](test_context& ctx) {
        auto directory = temporary_directory("cli-inspect");
        std::string output;
        ctx.equal(run_cli({"-C", directory.string(), "create", "a", "1"}, "", output), 0);
        ctx.equal(run_cli({"-C", directory.string(), "create", "b", "2"}, "", output), 0);
        ctx.equal(run_cli({"-C", directory.string(), "inspect"}, "", output), 0);
        ctx.equal(output, std::string{R"({"a":1,"b":2})" "\n"});
    }},
    {"checkpoint and verify succeed against a fresh heap", [](test_context& ctx) {
        auto directory = temporary_directory("cli-verify");
        std::string output;
        ctx.equal(run_cli({"-C", directory.string(), "checkpoint"}, "", output), 0);
        ctx.equal(run_cli({"-C", directory.string(), "verify"}, "", output), 0);
        ctx.check(output.find("\"ok\":true") != std::string::npos);
    }},
    {"malformed invocation prints usage on stderr and returns exit code 2", [](test_context& ctx) {
        auto directory = temporary_directory("cli-usage");
        std::istringstream in{""};
        std::ostringstream out;
        std::ostringstream err;
        const auto code = opheap::module::cli::execute(
            std::vector<std::string_view>{"-C", directory.string(), "create", "doc"}, in, out, err);
        ctx.check(code == 2);
        ctx.check(err.str().find("usage:") != std::string::npos);
    }},
    {"help with no arguments returns exit code 2 without opening a heap", [](test_context& ctx) {
        std::istringstream in{""};
        std::ostringstream out;
        std::ostringstream err;
        const auto code = opheap::module::cli::execute(std::vector<std::string_view>{}, in, out, err);
        ctx.check(code == 2);
        ctx.check(out.str().find("usage:") != std::string::npos);
    }},
    }) {}
};

inline static cli_test cli_test_instance;

} // namespace opheap::testing
