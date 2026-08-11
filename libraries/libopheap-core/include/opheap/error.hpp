#pragma once

#include <stdexcept>

namespace opheap {

struct error : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

} // namespace opheap
