#pragma once

#include "opheap/detail/loc.hpp"
#include "opheap/ids.hpp"

#include <string>

namespace opheap::detail {

struct recovered_update {
    std::string name;
    version_type expected_version{};
    version_type new_version{};
    std::string type;
    loc payload;
};

} // namespace opheap::detail
