#pragma once
#include "test_group.hpp"
#include "test_observable.hpp"
#include "test_support.hpp"

namespace opheap::testing {

struct observer_test : public test_group {
    observer_test() : test_group("observer", {
    {"binding routes change events", [](test_context& ctx) {
        recording_observer observer;
        test_observable item;
        item.bind({&observer, 7});
        item.touch();
        ctx.equal(observer.events.size(), std::size_t{1});
        ctx.equal(observer.events[0].root, root_token{7});
        ctx.equal(observer.events[0].offset, std::size_t{4});
        ctx.equal(observer.events[0].size, std::size_t{8});
    }},
    }) {}
};

inline static observer_test observer_test_instance;

} // namespace opheap::testing
