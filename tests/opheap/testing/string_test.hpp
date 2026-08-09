#pragma once
#include "test_group.hpp"
#include "test_support.hpp"

namespace opheap::testing {

inline static test_group string_test{"string", {
    {"mutations notify", [](test_context& ctx) {
        recording_observer observer;
        string text{"ab"};
        text.bind({&observer, 3});
        text.append("cd");
        text.push_back('e');
        text.erase(0, 1);
        ctx.equal(text.view(), std::string_view{"bcde"});
        ctx.equal(observer.events.size(), std::size_t{3});
    }},
    {"clear is idempotent", [](test_context& ctx) {
        recording_observer observer;
        string text;
        text.bind({&observer, 3});
        text.clear();
        ctx.check(observer.events.empty());
    }},
}};

} // namespace opheap::testing
