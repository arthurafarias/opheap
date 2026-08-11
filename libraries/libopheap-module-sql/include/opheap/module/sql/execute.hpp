#pragma once

#include "opheap/module/sql/print_usage.hpp"
#include "opheap/module/sql/run.hpp"
#include "opheap/module/sql/usage_error.hpp"

#include <istream>
#include <ostream>
#include <span>
#include <string_view>

namespace opheap::module::sql {

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
