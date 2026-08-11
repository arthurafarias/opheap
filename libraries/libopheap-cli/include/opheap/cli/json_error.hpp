#pragma once

#include <stdexcept>

namespace opheap::cli {

// Raised for malformed command-line usage or malformed JSON input, as opposed to
// domain errors (missing root, etc.), so run() can tell the two apart and decide
// whether to print usage.
class json_error final : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

} // namespace opheap::cli
