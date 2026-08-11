#pragma once

#include "opheap/error.hpp"

namespace opheap {

struct type_error : public error { using error::error; };

} // namespace opheap
