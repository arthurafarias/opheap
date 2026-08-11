#pragma once

#include "opheap/module/sql/interpreter.hpp"
#include "opheap/module/sql/result.hpp"
#include "opheap/module/sql/statement_splitter.hpp"
#include "opheap/module/sql/usage_error.hpp"

#include <opheap/error.hpp>
#include <opheap/heap.hpp>
#include <opheap/heap_config.hpp>

#include <algorithm>
#include <cstddef>
#include <exception>
#include <istream>
#include <ostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace opheap::module::sql {

inline void print_usage(std::ostream& output) {
    output << "usage: opheap-sql <database-directory>\n";
}

inline void print_result(std::ostream& output, const result& res) {
    if (!res.columns.empty()) {
        std::vector<std::size_t> widths(res.columns.size());
        for (std::size_t i = 0; i < res.columns.size(); ++i) widths[i] = res.columns[i].size();

        std::vector<std::vector<std::string>> cells;
        cells.reserve(res.rows.size());
        for (const auto& row : res.rows) {
            std::vector<std::string> line;
            line.reserve(row.size());
            for (std::size_t i = 0; i < row.size(); ++i) {
                auto text = to_display_string(row[i]);
                widths[i] = std::max(widths[i], text.size());
                line.push_back(std::move(text));
            }
            cells.push_back(std::move(line));
        }

        auto print_row = [&](const std::vector<std::string>& values) {
            for (std::size_t i = 0; i < values.size(); ++i) {
                if (i != 0) output << " | ";
                output << values[i] << std::string(widths[i] - values[i].size(), ' ');
            }
            output << '\n';
        };
        print_row(res.columns);
        std::size_t rule_width = 0;
        for (auto width : widths) rule_width += width + 3;
        output << std::string(rule_width, '-') << '\n';
        for (const auto& row : cells) print_row(row);
    }
    if (!res.message.empty()) output << res.message << '\n';
}

inline bool is_blank(std::string_view text) {
    return text.find_first_not_of(" \t\r\n") == std::string_view::npos;
}

// Runs the opheap-sql REPL: reads statements terminated by ';' from `input`, executes
// them against a heap opened at the directory named by arguments[0], and writes
// results to `output`. Throws usage_error when the database directory argument is
// missing; callers that want process-style error formatting and exit codes should use
// execute() instead.
inline int run(std::span<const std::string_view> arguments, std::istream& input, std::ostream& output) {
    if (arguments.empty()) throw usage_error("missing <database-directory> argument");

    auto heap = opheap::heap::open(opheap::heap_config{.path = arguments[0]});
    interpreter interp{heap};

    output << "opheap-sql: minimal SQL over an opheap store. "
              "Statements end with ';'. .tables lists tables, .exit quits.\n";

    std::string buffer;
    std::string line;
    while (true) {
        output << (buffer.empty() ? "sql> " : "...> ");
        if (!std::getline(input, line)) break;

        if (buffer.empty()) {
            if (line == ".exit" || line == ".quit") break;
            if (line == ".tables") {
                print_result(output, interp.list_tables());
                continue;
            }
        }

        buffer += line;
        buffer += '\n';
        while (auto statement = extract_statement(buffer)) {
            auto [text, consumed] = *statement;
            buffer.erase(0, consumed);
            if (is_blank(text)) continue;
            try {
                print_result(output, interp.execute(text));
            } catch (const opheap::error& err) {
                output << "Error: " << err.what() << '\n';
            } catch (const std::exception& err) {
                output << "Error: " << err.what() << '\n';
            }
        }
        if (is_blank(buffer)) buffer.clear();
    }

    heap.checkpoint();
    return 0;
}

// Runs the REPL the way the opheap-sql process does: formats a thrown usage_error to
// `error` (with usage text) and returns a process exit code, instead of propagating
// exceptions.
inline int execute(std::span<const std::string_view> arguments, std::istream& input, std::ostream& output,
    std::ostream& error) {
    try {
        return run(arguments, input, output);
    } catch (const usage_error& failure) {
        error << "opheap-sql: " << failure.what() << '\n';
        print_usage(error);
        return 2;
    }
}

} // namespace opheap::module::sql
