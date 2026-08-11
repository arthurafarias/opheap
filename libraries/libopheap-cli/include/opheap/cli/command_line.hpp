#pragma once

#include "opheap/cli/root_path.hpp"
#include "opheap/cli/usage_error.hpp"

#include <opheap/heap.hpp>
#include <opheap/heap_config.hpp>
#include <opheap/utils/serialization/json/opheap_utils_serialization_json.hpp>
#include <opheap/value.hpp>

#include <filesystem>
#include <istream>
#include <iterator>
#include <ostream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace opheap::cli {

namespace json = opheap::utils::serialization::json;

inline void print_usage(std::ostream& output) {
    output <<
        "usage: opheap-cli [-C <heap-directory>] <command> [arguments]\n"
        "\n"
        "commands:\n"
        "  create <name> <json|->  create a named JSON root\n"
        "  get <path>             print a root or dotted object path as JSON\n"
        "  inspect                print all named roots as a JSON object\n"
        "  update <name> <json|-> replace an existing root\n"
        "  delete <name>          logically delete an existing root\n"
        "  checkpoint             compact the WAL into a snapshot\n"
        "  verify                 verify the snapshot and WAL\n";
}

inline std::string read_json_argument(std::string_view argument, std::istream& input) {
    if (argument != "-") return std::string{argument};
    return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

// Parses arguments and runs a single opheap-cli command against a heap opened for the
// call, writing results to `output`. Throws usage_error for usage mistakes and
// std::runtime_error for domain errors (missing roots, etc.); callers that want
// process-style error formatting and exit codes should use execute() instead.
inline int run(std::span<const std::string_view> arguments, std::istream& input, std::ostream& output) {
    std::filesystem::path heap_path{"."};
    std::size_t argument = 0;
    while (argument < arguments.size()) {
        const std::string_view option = arguments[argument];
        if (option != "-C" && option != "--path") break;
        if (++argument == arguments.size()) throw usage_error(std::string{option} + " requires a directory");
        heap_path = arguments[argument++];
    }
    if (argument == arguments.size() || arguments[argument] == "help" ||
        arguments[argument] == "--help" || arguments[argument] == "-h") {
        print_usage(output);
        return argument == arguments.size() ? 2 : 0;
    }

    const std::string_view command = arguments[argument++];
    auto heap = opheap::heap::open(opheap::heap_config{.path = std::move(heap_path)});

    if (command == "checkpoint") {
        if (argument != arguments.size()) throw usage_error("checkpoint takes no arguments");
        heap.checkpoint();
        return 0;
    }
    if (command == "verify") {
        if (argument != arguments.size()) throw usage_error("verify takes no arguments");
        const auto report = heap.check_integrity();
        output << "{\"ok\":" << (report.ok ? "true" : "false")
               << ",\"roots\":" << report.roots
               << ",\"journal_records\":" << report.journal_records
               << ",\"journal_bytes\":" << report.journal_bytes
               << ",\"last_sequence\":" << report.last_sequence
               << ",\"message\":";
        json::write_string(output, report.message);
        output << "}\n";
        return report.ok ? 0 : 1;
    }
    if (command == "inspect") {
        if (argument != arguments.size()) throw usage_error("inspect takes no arguments");
        auto transaction = heap.begin();
        output.put('{');
        bool first = true;
        for (const auto& name : heap.root_names()) {
            if (!first) output.put(',');
            first = false;
            json::write_string(output, name);
            output.put(':');
            json::write_json(output, transaction.root(name));
        }
        output << "}\n";
        return 0;
    }

    if (argument == arguments.size()) throw usage_error(std::string{command} + " requires a root name");
    const std::string_view name = arguments[argument++];
    if (command == "get") {
        if (argument != arguments.size()) throw usage_error("get takes one root name");
        auto transaction = heap.begin();
        json::write_json(output, get_path(transaction, name));
        output.put('\n');
        return 0;
    }
    if (name.empty()) throw usage_error("root name must not be empty");
    auto transaction = heap.begin();
    auto& root = transaction.root(name);
    if (command == "delete") {
        if (argument != arguments.size()) throw usage_error("delete takes one root name");
        if (root.is_null()) throw std::runtime_error("root not found: " + std::string{name});
        root = opheap::null;
        transaction.commit();
        return 0;
    }
    if (command != "create" && command != "update")
        throw usage_error("unknown command: " + std::string{command});
    if (argument == arguments.size() || argument + 1 != arguments.size())
        throw usage_error(std::string{command} + " requires a root name and one JSON value");
    if (command == "create" && !root.is_null())
        throw std::runtime_error("root already exists: " + std::string{name});
    if (command == "update" && root.is_null())
        throw std::runtime_error("root not found: " + std::string{name});

    const auto payload = read_json_argument(arguments[argument], input);
    try {
        root = json::json_parser{payload, root.resource()}.parse();
    } catch (const json::json_error& failure) {
        throw usage_error(failure.what());
    }
    transaction.commit();
    json::write_json(output, root);
    output.put('\n');
    return 0;
}

// Runs a command the way the opheap-cli process does: formats thrown errors to
// `error` (with usage on malformed invocations) and returns a process exit code,
// instead of propagating exceptions.
inline int execute(std::span<const std::string_view> arguments, std::istream& input, std::ostream& output,
    std::ostream& error) {
    try {
        return run(arguments, input, output);
    } catch (const usage_error& failure) {
        error << "opheap-cli: " << failure.what() << '\n';
        print_usage(error);
        return 2;
    } catch (const std::exception& failure) {
        error << "opheap-cli: " << failure.what() << '\n';
        return 1;
    }
}

} // namespace opheap::cli
