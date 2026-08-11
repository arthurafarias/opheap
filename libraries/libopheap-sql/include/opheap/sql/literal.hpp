#pragma once

#include <cstdint>
#include <string>
#include <variant>

namespace opheap::sql {

// A NULL value is represented by the std::monostate alternative.
using literal = std::variant<std::monostate, bool, std::int64_t, double, std::string>;

} // namespace opheap::sql
