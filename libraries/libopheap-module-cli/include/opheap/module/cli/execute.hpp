#pragma once

#include "opheap/module/cli/print_usage.hpp"
#include "opheap/module/cli/run.hpp"
#include "opheap/module/cli/usage_error.hpp"

#include <exception>
#include <istream>
#include <ostream>
#include <span>
#include <string_view>

namespace opheap::module::cli {

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

} // namespace opheap::module::cli
