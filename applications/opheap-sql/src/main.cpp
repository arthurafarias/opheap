#include <opheap/opheap.hpp>
#include <opheap/sql/opheap_sql.hpp>

#include <algorithm>
#include <cstdio>
#include <iostream>
#include <string>

// opheap-sql: an interactive REPL for a minimal SQL dialect (CREATE TABLE, INSERT,
// SELECT/WHERE/ORDER BY/LIMIT, UPDATE, DELETE) executed directly against an
// opheap-backed store. The dialect and its execution engine live in the header-only
// opheap::sql library under libraries/libopheap-sql; this file is only the terminal
// front end.

namespace {

void print_result(const opheap::sql::result& res) {
    if (!res.columns.empty()) {
        std::vector<std::size_t> widths(res.columns.size());
        for (std::size_t i = 0; i < res.columns.size(); ++i) widths[i] = res.columns[i].size();

        std::vector<std::vector<std::string>> cells;
        cells.reserve(res.rows.size());
        for (const auto& row : res.rows) {
            std::vector<std::string> line;
            line.reserve(row.size());
            for (std::size_t i = 0; i < row.size(); ++i) {
                auto text = opheap::sql::to_display_string(row[i]);
                widths[i] = std::max(widths[i], text.size());
                line.push_back(std::move(text));
            }
            cells.push_back(std::move(line));
        }

        auto print_row = [&](const std::vector<std::string>& values) {
            for (std::size_t i = 0; i < values.size(); ++i) {
                if (i != 0) std::cout << " | ";
                std::cout << values[i] << std::string(widths[i] - values[i].size(), ' ');
            }
            std::cout << '\n';
        };
        print_row(res.columns);
        std::size_t rule_width = 0;
        for (auto width : widths) rule_width += width + 3;
        std::cout << std::string(rule_width, '-') << '\n';
        for (const auto& row : cells) print_row(row);
    }
    if (!res.message.empty()) std::cout << res.message << '\n';
}

bool is_blank(std::string_view text) {
    return text.find_first_not_of(" \t\r\n") == std::string_view::npos;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: opheap-sql <database-directory>\n");
        return 1;
    }

    opheap::heap heap = opheap::heap::open({.path = argv[1]});
    opheap::sql::interpreter interpreter{heap};

    std::cout << "opheap-sql: minimal SQL over an opheap store. "
                 "Statements end with ';'. .tables lists tables, .exit quits.\n";

    std::string buffer;
    std::string line;
    while (true) {
        std::cout << (buffer.empty() ? "sql> " : "...> ");
        if (!std::getline(std::cin, line)) break;

        if (buffer.empty()) {
            if (line == ".exit" || line == ".quit") break;
            if (line == ".tables") {
                print_result(interpreter.list_tables());
                continue;
            }
        }

        buffer += line;
        buffer += '\n';
        while (auto statement = opheap::sql::extract_statement(buffer)) {
            auto [text, consumed] = *statement;
            buffer.erase(0, consumed);
            if (is_blank(text)) continue;
            try {
                print_result(interpreter.execute(text));
            } catch (const opheap::error& err) {
                std::cout << "Error: " << err.what() << '\n';
            } catch (const std::exception& err) {
                std::cout << "Error: " << err.what() << '\n';
            }
        }
        if (is_blank(buffer)) buffer.clear();
    }

    heap.checkpoint();
    return 0;
}
