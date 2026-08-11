#pragma once

#include <cstdint>

namespace opheap::detail {

enum class source : std::uint8_t { snapshot, journal };

} // namespace opheap::detail
