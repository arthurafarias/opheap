#pragma once

#include "opheap/change_observer.hpp"
#include "opheap/ids.hpp"

namespace opheap {

struct binding {
    change_observer* observer{};
    root_token root{};
};

} // namespace opheap
