#pragma once

#include <cstddef>
#include <vector>

namespace opheap::detail {

struct encoded_update {
    std::vector<std::byte> bytes;
    std::size_t payload_offset{};
};

} // namespace opheap::detail
