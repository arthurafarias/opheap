#pragma once

#include "opheap/ids.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace opheap::detail {

struct root_update {
    std::string name;
    version_type expected_version{};
    version_type new_version{};
    std::string type;
    std::vector<std::byte> payload;
};

} // namespace opheap::detail
