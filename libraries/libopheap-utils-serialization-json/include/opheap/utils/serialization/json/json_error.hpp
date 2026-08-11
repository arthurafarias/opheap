#pragma once

#include <stdexcept>

namespace opheap::utils::serialization::json {

// Raised for malformed JSON input encountered while parsing.
class json_error final : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

} // namespace opheap::utils::serialization::json
