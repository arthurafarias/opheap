// Hammers a single shared root from many threads racing optimistic commits.
// opheap's commit path is: snapshot expected_version at transaction::root(),
// then heap_state::commit() rejects with conflict_error if that version moved
// under it. Under heavy contention almost every commit should lose the race
// at least once. This scenario proves that despite that churn no update is
// ever lost or double-applied: retried until success, the final counter must
// equal exactly the number of commits that actually succeeded.
#include "../support.hpp"

#include <opheap/opheap.hpp>
#include <opheap/testing/temporary_directory.hpp>

#include <atomic>
#include <cstdint>
#include <thread>
#include <vector>

namespace {

OPHEAP_STRESS_SCENARIO(conflict_storm) {
    auto directory = opheap::testing::temporary_directory("stress-conflict-storm");
    auto heap = opheap::heap::open({.path = directory});
    {
        auto seed = heap.begin();
        seed.object_root()["n"] = 0;
        seed.commit();
    }

    const auto thread_count = 4U * opts.scale;
    const auto increments_per_thread = 200U * opts.scale;

    std::atomic<std::uint64_t> successes{0};
    std::atomic<std::uint64_t> conflicts{0};

    std::vector<std::thread> threads;
    threads.reserve(thread_count);
    for (unsigned t = 0; t < thread_count; ++t) {
        threads.emplace_back([&] {
            for (unsigned i = 0; i < increments_per_thread; ++i) {
                for (;;) {
                    auto tx = heap.begin();
                    auto& root = tx.object_root();
                    const auto current = root.at("n").as_integer();
                    root["n"] = current + 1;
                    try {
                        tx.commit();
                        successes.fetch_add(1, std::memory_order_relaxed);
                        break;
                    } catch (const opheap::conflict_error&) {
                        conflicts.fetch_add(1, std::memory_order_relaxed);
                    }
                }
            }
        });
    }
    for (auto& thread : threads) thread.join();

    const auto expected = static_cast<std::int64_t>(successes.load());
    auto read = heap.begin();
    ctx.equal(read.object_root().at("n").as_integer(), expected,
               "lost or double-applied update under contention");
    ctx.equal(successes.load(), static_cast<std::uint64_t>(thread_count) * increments_per_thread,
               "not every increment eventually succeeded");
    ctx.check(heap.check_integrity().ok, "heap failed integrity check after contention");

    std::cout << "  successes=" << successes.load() << " conflicts=" << conflicts.load() << '\n';
}

} // namespace
