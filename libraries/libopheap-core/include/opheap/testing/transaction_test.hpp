#pragma once
#include "test_group.hpp"
#include "test_support.hpp"

namespace opheap::testing {

struct transaction_test : public test_group {
    transaction_test() : test_group("transaction", {
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
    {"root name must not be empty", [](test_context& ctx) {
        auto directory = temporary_directory("tx-empty-name");
        auto heap = opheap::heap::open({.path = directory});
        auto tx = heap.begin();
        ctx.throws<transaction_error>([&] { (void)tx.root(""); });
    }},
    {"an inactive transaction rejects further use", [](test_context& ctx) {
        auto directory = temporary_directory("tx-inactive");
        auto heap = opheap::heap::open({.path = directory});
        auto tx = heap.begin();
        tx.object_root()["n"] = 1;
        tx.commit();
        ctx.throws<transaction_error>([&] { (void)tx.root(); });
        ctx.throws<transaction_error>([&] { tx.commit(); });
    }},
    {"root() rejects a payload with trailing bytes past the encoded value", [](test_context& ctx) {
        auto directory = temporary_directory("tx-trailing-payload");
        {
            auto backend = make_default_storage_backend();
            detail::heap_state state{opheap::heap_config{.path = directory}, backend};
            detail::writer writer;
            codec<value>::encode(writer, value{5});
            auto payload = std::move(writer).take();
            payload.push_back(std::byte{0}); // corrupt: trailing byte past the encoded value
            detail::root_update update{"bad", 0, 1, std::string{codec<value>::type_name}, std::move(payload)};
            state.commit(1, {update});
        }
        auto heap = opheap::heap::open({.path = directory});
        auto tx = heap.begin();
        ctx.throws<corruption_error>([&] { (void)tx.root("bad"); });
    }},
    }) {}
};

inline static transaction_test transaction_test_instance;

} // namespace opheap::testing
