#include <opheap/module/cli/opheap_cli.hpp>

#include <iostream>
#include <string_view>
#include <vector>

// opheap-cli: a small tool for creating, reading, updating, and deleting JSON roots
// in an opheap store from the shell. Argument parsing, JSON encoding/decoding, and
// command dispatch live in the header-only opheap::module::cli library under
// libraries/libopheap-module-cli; this file is only the process front end that turns
// argv/stdin/stdout into calls against that library.

int main(int argc, char** argv) {
    std::vector<std::string_view> arguments(argv + 1, argv + argc);
    return opheap::module::cli::execute(arguments, std::cin, std::cout, std::cerr);
}
