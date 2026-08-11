#pragma once

#include <stdexcept>

namespace opheap::module::cli {

// Raised for malformed command-line usage, as opposed to domain errors (missing
// root, etc.), so run() can tell the two apart and decide whether to print usage.
// Malformed JSON input is translated into a usage_error at the point it's parsed
// so both failure modes get the same handling.
class usage_error final : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

} // namespace opheap::module::cli
