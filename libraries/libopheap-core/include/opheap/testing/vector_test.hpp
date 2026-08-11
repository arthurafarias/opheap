#pragma once
#include "test_group.hpp"
#include "test_support.hpp"

namespace opheap::testing {

struct vector_test : public test_group {
    vector_test() : test_group("vector", {
    {"structure and element changes notify", [](test_context& ctx) {
        recording_observer observer;
        vector<property<int>> values;
        values.bind({&observer, 4});
        values.emplace_back(10);
        values[0] = 11;
        values.push_back(property<int>{20});
        ctx.equal(values.size(), std::size_t{2});
        ctx.check(observer.events.size() >= 3);
    }},
    {"reallocation keeps child bindings", [](test_context& ctx) {
        recording_observer observer;
        vector<property<int>> values;
        values.bind({&observer, 4});
        for (int i = 0; i < 64; ++i) values.emplace_back(i);
        const auto before = observer.events.size();
        values[1] = 999;
        ctx.equal(observer.events.size(), before + 1);
    }},
    }) {}
};

inline static vector_test vector_test_instance;

} // namespace opheap::testing
