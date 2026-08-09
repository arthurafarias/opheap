#pragma once
#include "test_group.hpp"
#include "test_support.hpp"

#include <memory_resource>
#include <vector>

namespace opheap::testing {

inline static test_group memory_resource_test{"observing_resource", {
    {"decorates pmr allocation", [](test_context& ctx) {
        recording_observer observer;
        observing_resource resource{std::pmr::get_default_resource(), {&observer, 10}};
        {
            std::pmr::vector<int> values{&resource};
            values.reserve(128);
            values.push_back(1);
        }
        ctx.check(!observer.events.empty());
        ctx.equal(observer.events.front().kind, change_kind::allocation);
        ctx.equal(observer.events.back().kind, change_kind::deallocation);
    }},
}};

} // namespace opheap::testing
