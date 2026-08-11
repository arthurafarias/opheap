#pragma once

#include "opheap/change_event.hpp"

namespace opheap {

struct change_observer {
public:
    virtual ~change_observer() = default;
    virtual void changed(const change_event& event) noexcept = 0;
};

} // namespace opheap
