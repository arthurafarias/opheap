#pragma once

#include "opheap/error.hpp"

namespace opheap {

struct corruption_error : public error { using error::error; };

} // namespace opheap
