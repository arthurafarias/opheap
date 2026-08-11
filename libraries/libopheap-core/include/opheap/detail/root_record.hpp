#pragma once

#include "opheap/detail/loc.hpp"
#include "opheap/ids.hpp"

#include <string>

namespace opheap::detail {

struct root_record {
    version_type version{};
    std::string type;
    loc payload;
};

} // namespace opheap::detail
