#pragma once

#include "opheap/detail/root_record.hpp"
#include "opheap/ids.hpp"

#include <string>
#include <unordered_map>

namespace opheap::detail {

struct snapshot_image {
    std::unordered_map<std::string, root_record> roots;
    sequence_number sequence{};
};

} // namespace opheap::detail
