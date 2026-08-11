#pragma once

#include <cstdint>

namespace opheap::detail {

enum class record_type : std::uint16_t { begin = 1, root_update = 2, commit = 3 };

} // namespace opheap::detail
