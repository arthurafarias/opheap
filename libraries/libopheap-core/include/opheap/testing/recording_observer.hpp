#pragma once

#include <opheap/opheap.hpp>

#include <vector>

namespace opheap::testing {

struct recording_observer final : public change_observer {
public:
    void changed(const change_event& event) noexcept override { events.push_back(event); }
    std::vector<change_event> events;
};

} // namespace opheap::testing
