#pragma once

#include "opheap/error.hpp"

namespace opheap::sql {

struct sql_error : public opheap::error { using opheap::error::error; };

} // namespace opheap::sql
