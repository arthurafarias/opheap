#pragma once

#include "opheap/error.hpp"

namespace opheap::module::sql {

struct sql_error : public opheap::error { using opheap::error::error; };

} // namespace opheap::module::sql
