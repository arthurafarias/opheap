#pragma once

#include "opheap/ids.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

namespace opheap {

struct integrity_report {
    bool ok{true};
    std::size_t roots{};
    std::size_t journal_records{};
    std::uint64_t journal_bytes{};
    sequence_number last_sequence{};
    std::string message{"ok"};
};

} // namespace opheap
