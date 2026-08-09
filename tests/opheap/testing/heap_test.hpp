#pragma once
#include "test_group.hpp"
#include "test_support.hpp"

#include <fstream>
#include <thread>
#include <vector>

namespace opheap::testing {

inline static test_group heap_test{"heap", {
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
}};

} // namespace opheap::testing
