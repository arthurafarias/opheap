#include <opheap/module/sql/opheap_sql.hpp>

#include <iostream>
#include <string_view>
#include <vector>

// opheap-sql: an interactive REPL for a minimal SQL dialect (CREATE TABLE, INSERT,
// SELECT/WHERE/ORDER BY/LIMIT, UPDATE, DELETE) executed directly against an
// opheap-backed store. The dialect, execution engine, and REPL loop live in the
// header-only opheap::module::sql library under libraries/libopheap-module-sql; this
// file is only the process front end that turns argv/stdin/stdout into calls against
// that library.

int main(int argc, char** argv) {
    std::vector<std::string_view> arguments(argv + 1, argv + argc);
    return opheap::module::sql::execute(arguments, std::cin, std::cout, std::cerr);
}
