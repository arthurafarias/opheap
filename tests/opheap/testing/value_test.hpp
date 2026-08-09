#pragma once
#include "test_group.hpp"
#include "test_support.hpp"

namespace opheap::testing {

inline static test_group value_test{"value", {
    {"javascript-like object tree", [](test_context& ctx) {
        recording_observer observer;
        value root;
        root.bind({&observer, 6});
        root["users"]["arthur"]["age"] = 42;
        root["users"]["arthur"]["name"] = "Arthur";
        root["users"]["arthur"]["roles"].as_array().push_back(value{"admin"});
        ctx.equal(root.at("users").at("arthur").at("age").as_integer(), std::int64_t{42});
        ctx.equal(root.at("users").at("arthur").at("roles").as_array().size(), std::size_t{1});
        ctx.check(!observer.events.empty());
    }},
    {"type mismatch throws", [](test_context& ctx) {
        const value item{42};
        ctx.throws<type_error>([&] { (void)item.as_string(); });
    }},
    {"move assignment keeps destination observer", [](test_context& ctx) {
        recording_observer observer;
        value item;
        item.bind({&observer, 9});
        item = value{"hello"};
        const auto before = observer.events.size();
        item.as_string().append(" world");
        ctx.equal(observer.events.size(), before + 1);
    }},
}};

} // namespace opheap::testing
