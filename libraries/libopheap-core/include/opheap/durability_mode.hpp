#pragma once

#include <cstdint>

namespace opheap {

// Strict durability is the default: commit acknowledges only after a persistence barrier.
enum class durability_mode : std::uint8_t {
    strict,
    relaxed
};

} // namespace opheap
