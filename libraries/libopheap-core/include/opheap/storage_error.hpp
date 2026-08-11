#pragma once

#include "opheap/error.hpp"

namespace opheap {

struct storage_error : public error { using error::error; };

} // namespace opheap
