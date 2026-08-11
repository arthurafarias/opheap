#pragma once

#include "opheap/detail/root_record.hpp"
#include "opheap/ids.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>

namespace opheap::detail {

struct recovery_result {
    std::unordered_map<std::string, root_record> roots;
    sequence_number last_sequence{};
    std::size_t records{};
    std::uint64_t valid_bytes{};
};

} // namespace opheap::detail
