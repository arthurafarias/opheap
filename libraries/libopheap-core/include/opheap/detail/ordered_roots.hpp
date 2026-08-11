#pragma once

#include "opheap/detail/indexed_root.hpp"
#include "opheap/detail/snapshot_image.hpp"
#include "opheap/storage_error.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <vector>

namespace opheap::detail {

inline std::vector<indexed_root> ordered_roots(const snapshot_image& image) {
    std::vector<indexed_root> roots;
    roots.reserve(image.roots.size());
    for (const auto& [name, record] : image.roots) roots.push_back({name, record, 0});
    std::sort(roots.begin(), roots.end(), [](const auto& a, const auto& b) {
        return a.name < b.name;
    });
    std::uint64_t offset = 0;
    for (auto& root : roots) {
        root.relative_offset = offset;
        if (root.record.payload.size > std::numeric_limits<std::uint64_t>::max() - offset) {
            throw storage_error("snapshot payload size overflow");
        }
        offset += root.record.payload.size;
    }
    return roots;
}

} // namespace opheap::detail
