#pragma once

#include "opheap/detail/root_record.hpp"

#include <cstdint>
#include <string>

namespace opheap::detail {

struct indexed_root {
    std::string name;
    root_record record;
    std::uint64_t relative_offset{};
};

} // namespace opheap::detail
