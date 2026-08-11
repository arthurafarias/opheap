#pragma once

#include <cstdint>

namespace opheap {

enum class change_kind : std::uint8_t {
    value,
    structure,
    allocation,
    deallocation
};

} // namespace opheap
