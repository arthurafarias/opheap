#pragma once
#include "test_group.hpp"
#include "test_support.hpp"

namespace opheap::testing {

struct property_test : public test_group {
    property_test() : test_group("property", {
    {"assignment and arithmetic are observable", [](test_context& ctx) {
        recording_observer observer;
        property<int> value{10};
        value.bind({&observer, 1});
        value = 11;
        value += 4;
        ++value;
        ctx.equal(value.get(), 16);
        ctx.equal(observer.events.size(), std::size_t{3});
    }},
    {"equal assignment is coalesced", [](test_context& ctx) {
        recording_observer observer;
        property<int> value{10};
        value.bind({&observer, 1});
        value = 10;
        ctx.check(observer.events.empty());
    }},
    {"mutate closes the mutable escape hatch", [](test_context& ctx) {
        recording_observer observer;
        property<std::string> value{"abc"};
        value.bind({&observer, 2});
        value.mutate([](auto& text) { text += "def"; });
        ctx.equal(value.get(), std::string{"abcdef"});
        ctx.equal(observer.events.size(), std::size_t{1});
    }},
    }) {}
};

inline static property_test property_test_instance;

} // namespace opheap::testing
