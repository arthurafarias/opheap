#pragma once
#include "test_group.hpp"
#include "test_support.hpp"

namespace opheap::testing {

struct value_test : public test_group {
    value_test() : test_group("value", {
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
    {"every scalar accessor rejects a mismatched type", [](test_context& ctx) {
        const value integer{42};
        ctx.throws<type_error>([&] { (void)integer.as_bool(); });
        ctx.throws<type_error>([&] { (void)integer.as_number(); });
        const value string_value{"text"};
        ctx.throws<type_error>([&] { (void)string_value.as_integer(); });
        const value boolean{true};
        ctx.throws<type_error>([&] { (void)boolean.as_array(); });
        ctx.throws<type_error>([&] { (void)boolean.as_object(); });
    }},
    {"at() rejects a non-object value and an absent key", [](test_context& ctx) {
        const value scalar{42};
        ctx.throws<type_error>([&] { (void)scalar.at("missing"); });
        value root;
        root["present"] = 1;
        const value& const_root = root;
        ctx.throws<std::out_of_range>([&] { (void)const_root.at("absent"); });
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
    }) {}
};

inline static value_test value_test_instance;

} // namespace opheap::testing
