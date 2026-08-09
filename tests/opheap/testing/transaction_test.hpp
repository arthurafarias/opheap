#pragma once
#include "test_group.hpp"
#include "test_support.hpp"

namespace opheap::testing {

inline static test_group transaction_test{"transaction", {
    {"read your writes and dirty coalescing", [](test_context& ctx) {
        auto directory = temporary_directory("tx");
        auto heap = opheap::heap::open({.path = directory});
        auto tx = heap.begin();
        auto& root = tx.object_root();
        root["counter"] = 1;
        root["counter"] = 2;
        root["counter"] = 3;
        ctx.equal(root.at("counter").as_integer(), std::int64_t{3});
        ctx.equal(tx.dirty_roots(), std::size_t{1});
        tx.commit();
        ctx.check(!tx.active());
    }},
    {"abort publishes nothing", [](test_context& ctx) {
        auto directory = temporary_directory("abort");
        auto heap = opheap::heap::open({.path = directory});
        {
            auto tx = heap.begin();
            tx.object_root()["secret"] = "not committed";
            tx.abort();
        }
        auto read = heap.begin();
        auto& root = read.root();
        ctx.check(root.is_null());
    }},
    {"optimistic conflict is deterministic", [](test_context& ctx) {
        auto directory = temporary_directory("conflict");
        auto heap = opheap::heap::open({.path = directory});
        {
            auto seed = heap.begin(); seed.object_root()["n"] = 0; seed.commit();
        }
        auto a = heap.begin();
        auto b = heap.begin();
        a.object_root()["n"] = 1;
        b.object_root()["n"] = 2;
        a.commit();
        ctx.throws<conflict_error>([&] { b.commit(); });
    }},
}};

} // namespace opheap::testing
