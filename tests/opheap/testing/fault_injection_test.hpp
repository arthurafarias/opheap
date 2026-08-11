#pragma once
#include "fault_backend.hpp"
#include "test_group.hpp"
#include "test_support.hpp"

#include <memory>

namespace opheap::testing {

inline static test_group fault_injection_test{"fault_injection", {
    {"partial commit append is recovered as uncommitted tail", [](test_context& ctx) {
        auto directory = temporary_directory("fault-append");
        auto plan = std::make_shared<fault_plan>();
        plan->partial_append_once = true;
        auto backend = std::make_shared<fault_backend>(make_default_storage_backend(), plan);
        {
            auto heap = opheap::heap::open({.path = directory}, backend);
            auto tx = heap.begin();
            tx.object_root()["must_not_publish"] = true;
            ctx.throws<storage_error>([&] { tx.commit(); });
        }
        auto recovered = opheap::heap::open({.path = directory});
        auto tx = recovered.begin();
        ctx.check(tx.root().is_null());
    }},
    {"failed durability barrier poisons live heap", [](test_context& ctx) {
        auto directory = temporary_directory("fault-flush");
        auto plan = std::make_shared<fault_plan>();
        plan->fail_flush_data_once = true;
        auto backend = std::make_shared<fault_backend>(make_default_storage_backend(), plan);
        auto heap = opheap::heap::open({.path = directory}, backend);
        auto tx = heap.begin();
        tx.object_root()["n"] = 1;
        ctx.throws<storage_error>([&] { tx.commit(); });
        auto next = heap.begin();
        next.object_root("other")["n"] = 2;
        ctx.throws<storage_error>([&] { next.commit(); });
    }},
    {"interrupted checkpoint leaves journal recovery path", [](test_context& ctx) {
        auto directory = temporary_directory("fault-checkpoint");
        auto plan = std::make_shared<fault_plan>();
        auto backend = std::make_shared<fault_backend>(make_default_storage_backend(), plan);
        {
            auto heap = opheap::heap::open({.path = directory}, backend);
            auto tx = heap.begin(); tx.object_root()["n"] = 77; tx.commit();
            plan->fail_replace_once = true;
            ctx.throws<storage_error>([&] { heap.checkpoint(); });
        }
        auto recovered = opheap::heap::open({.path = directory});
        auto read = recovered.begin();
        ctx.equal(read.object_root().at("n").as_integer(), std::int64_t{77});
    }},
}};

} // namespace opheap::testing
