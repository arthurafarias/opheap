#pragma once

#include "opheap/durability_mode.hpp"

#include <cstddef>
#include <filesystem>

namespace opheap {

struct heap_config {
    std::filesystem::path path;
    std::size_t checkpoint_journal_bytes{64U * 1024U * 1024U};
    durability_mode durability{durability_mode::strict};
    bool checksums{true};
    std::size_t cache_bytes{8U * 1024U * 1024U};
};

} // namespace opheap
