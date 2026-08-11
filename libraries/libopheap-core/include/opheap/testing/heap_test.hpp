#pragma once
#include "test_group.hpp"
#include "test_support.hpp"

#include <fstream>
#include <thread>
#include <vector>

namespace opheap::testing {

struct heap_test : public test_group {
    heap_test() : test_group("heap", {
    {"persistent object tree survives restart", [](test_context& ctx) {
        auto directory = temporary_directory("restart");
        {
            auto heap = opheap::heap::open({.path = directory});
            auto tx = heap.begin();
            auto& state = tx.object_root();
            state["users"]["arthur"]["age"] = 42;
            state["users"]["arthur"]["name"] = "Arthur";
            tx.commit();
        }
        {
            auto heap = opheap::heap::open({.path = directory});
            auto tx = heap.begin();
            const auto& state = tx.object_root();
            ctx.equal(state.at("users").at("arthur").at("age").as_integer(), std::int64_t{42});
            ctx.equal(state.at("users").at("arthur").at("name").as_string().view(), std::string_view{"Arthur"});
        }
    }},
    {"checkpoint bounds journal and recovery remains idempotent", [](test_context& ctx) {
        auto directory = temporary_directory("checkpoint");
        {
            auto heap = opheap::heap::open({.path = directory});
            auto tx = heap.begin(); tx.object_root()["n"] = 7; tx.commit();
            heap.checkpoint();
            ctx.equal(std::filesystem::file_size(directory / "heap.wal"), std::uintmax_t{0});
        }
        for (int pass = 0; pass < 3; ++pass) {
            auto heap = opheap::heap::open({.path = directory});
            auto tx = heap.begin();
            ctx.equal(tx.object_root().at("n").as_integer(), std::int64_t{7});
        }
    }},
    {"integrity detects checksum corruption", [](test_context& ctx) {
        auto directory = temporary_directory("corruption");
        {
            auto heap = opheap::heap::open({.path = directory});
            auto tx = heap.begin(); tx.object_root()["n"] = 8; tx.commit();
        }
        auto path = directory / "heap.wal";
        auto bytes = std::filesystem::file_size(path);
        ctx.check(bytes > 16);
        std::fstream file(path, std::ios::in | std::ios::out | std::ios::binary);
        file.seekg(static_cast<std::streamoff>(bytes - 1));
        char byte{}; file.read(&byte, 1); byte ^= 0x55;
        file.seekp(static_cast<std::streamoff>(bytes - 1)); file.write(&byte, 1); file.flush();
        auto backend = make_default_storage_backend();
        // Opening itself must reject committed-record corruption.
        ctx.throws<corruption_error>([&] {
            auto broken = opheap::heap::open({.path = directory}, backend);
            (void)broken;
        });
    }},
    {"multiple threads commit independent roots", [](test_context& ctx) {
        auto directory = temporary_directory("threads");
        auto heap = opheap::heap::open({.path = directory});
        std::vector<std::thread> threads;
        for (int thread = 0; thread < 8; ++thread) {
            threads.emplace_back([&, thread] {
                auto tx = heap.begin();
                tx.object_root("thread-" + std::to_string(thread))["value"] = thread;
                tx.commit();
            });
        }
        for (auto& thread : threads) thread.join();
        auto read = heap.begin();
        for (int thread = 0; thread < 8; ++thread) {
            ctx.equal(read.object_root("thread-" + std::to_string(thread)).at("value").as_integer(),
                      static_cast<std::int64_t>(thread));
        }
    }},
    {"checkpoint reopen loads roots on demand through bounded cache", [](test_context& ctx) {
        auto directory = temporary_directory("lazy-cache");
        const std::string payload(900, 'x');
        {
            auto heap = opheap::heap::open({.path = directory, .cache_bytes = 1300});
            for (const auto* name : {"a", "b", "c"}) {
                auto tx = heap.begin();
                tx.object_root(name)["data"] = payload;
                tx.commit();
            }
            heap.checkpoint();
        }

        auto heap = opheap::heap::open({.path = directory, .cache_bytes = 1300});
        ctx.equal(heap.root_count(), std::size_t{3});
        ctx.equal(heap.cache().bytes, std::size_t{0});
        ctx.equal(heap.cache().entries, std::size_t{0});

        {
            auto tx = heap.begin();
            ctx.equal(tx.object_root("a").at("data").as_string().size(), payload.size());
        }
        const auto after_a = heap.cache();
        ctx.equal(after_a.entries, std::size_t{1});
        ctx.check(after_a.bytes <= 1300);
        ctx.equal(after_a.misses, std::uint64_t{1});

        {
            auto tx = heap.begin();
            ctx.equal(tx.object_root("a").at("data").as_string().size(), payload.size());
        }
        ctx.equal(heap.cache().hits, std::uint64_t{1});

        {
            auto tx = heap.begin();
            (void)tx.object_root("b").at("data").as_string().size();
        }
        {
            auto tx = heap.begin();
            (void)tx.object_root("c").at("data").as_string().size();
        }
        const auto bounded = heap.cache();
        ctx.check(bounded.bytes <= 1300);
        ctx.equal(bounded.entries, std::size_t{1});
        ctx.check(bounded.evictions >= 2);
    }},
    {"oversized root is demand loaded but never retained by cache", [](test_context& ctx) {
        auto directory = temporary_directory("oversized-cache");
        const std::string payload(4096, 'z');
        {
            auto heap = opheap::heap::open({.path = directory, .cache_bytes = 512});
            auto tx = heap.begin();
            tx.object_root()["data"] = payload;
            tx.commit();
            heap.checkpoint();
        }
        auto heap = opheap::heap::open({.path = directory, .cache_bytes = 512});
        auto tx = heap.begin();
        ctx.equal(tx.object_root().at("data").as_string().size(), payload.size());
        ctx.equal(heap.cache().bytes, std::size_t{0});
        ctx.equal(heap.cache().entries, std::size_t{0});
    }},
    {"independent roots can commit from concurrent snapshots", [](test_context& ctx) {
        auto directory = temporary_directory("roots");
        auto heap = opheap::heap::open({.path = directory});
        auto a = heap.begin();
        auto b = heap.begin();
        a.object_root("a")["v"] = 1;
        b.object_root("b")["v"] = 2;
        a.commit();
        b.commit();
        auto read = heap.begin();
        ctx.equal(read.object_root("a").at("v").as_integer(), std::int64_t{1});
        ctx.equal(read.object_root("b").at("v").as_integer(), std::int64_t{2});
    }},
    }) {}
};

inline static heap_test heap_test_instance;

} // namespace opheap::testing
