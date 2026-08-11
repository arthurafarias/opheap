#pragma once

#include "opheap/error.hpp"

namespace opheap {

struct conflict_error : public error { using error::error; };

} // namespace opheap
