#pragma once

#include <vector>

namespace opheap::testing {

struct test_group;

inline std::vector<const test_group*>& registry() {
    static std::vector<const test_group*> groups;
    return groups;
}

} // namespace opheap::testing
