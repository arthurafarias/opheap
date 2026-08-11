#pragma once

#include "opheap/change_kind.hpp"
#include "opheap/ids.hpp"

#include <cstddef>

namespace opheap {

struct change_event {
    root_token root{};
    change_kind kind{change_kind::value};
    std::size_t offset{};
    std::size_t size{};
};

} // namespace opheap
