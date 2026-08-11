#pragma once

#include <cstddef>
#include <cstdint>

namespace opheap {

struct cache_info {
    std::size_t bytes{};
    std::size_t entries{};
    std::uint64_t hits{};
    std::uint64_t misses{};
    std::uint64_t evictions{};
};

} // namespace opheap
