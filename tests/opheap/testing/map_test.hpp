#pragma once
#include "test_group.hpp"
#include "test_support.hpp"

namespace opheap::testing {

inline static test_group map_test{"map", {
    {"insert erase and mapped mutation notify", [](test_context& ctx) {
        recording_observer observer;
        map<std::string, property<int>> values;
        values.bind({&observer, 5});
        values["a"] = 10;
        values["a"] += 1;
        values.erase("a");
        ctx.check(observer.events.size() >= 3);
        ctx.check(values.empty());
    }},
    {"existing operator index does not report structure", [](test_context& ctx) {
        recording_observer observer;
        map<std::string, property<int>> values;
        values.bind({&observer, 5});
        (void)values["a"];
        const auto count = observer.events.size();
        (void)values["a"];
        ctx.equal(observer.events.size(), count);
    }},
}};

} // namespace opheap::testing
