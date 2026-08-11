#pragma once

#include "opheap/error.hpp"

namespace opheap {

struct transaction_error : public error { using error::error; };

} // namespace opheap
