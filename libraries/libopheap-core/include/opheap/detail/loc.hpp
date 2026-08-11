#pragma once

#include "opheap/detail/source.hpp"

#include <cstdint>

namespace opheap::detail {

struct loc {
    source kind{};
    std::uint64_t offset{};
    std::uint64_t size{};
    std::uint32_t checksum{};
};

} // namespace opheap::detail
