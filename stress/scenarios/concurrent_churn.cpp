// Runs writers (each owning a private slice of roots), readers picking random
// roots, and a background thread calling heap::checkpoint() -- all
// concurrently, against a cache deliberately sized far below the working set
// so every read forces eviction/reload traffic. Exercises the interaction
// between commit_mutex, roots_mutex and the bounded payload cache under real
// contention, which the small fixed-thread-count unit tests barely touch.
#include "../support.hpp"

#include <opheap/opheap.hpp>
#include <opheap/testing/temporary_directory.hpp>

#include <atomic>
#include <cstdint>
#include <random>
#include <string>
#include <thread>
#include <vector>

namespace {

OPHEAP_STRESS_SCENARIO(concurrent_churn) {
    auto directory = opheap::testing::temporary_directory("stress-churn");
    auto heap = opheap::heap::open({
        .path = directory,
        .checkpoint_journal_bytes = 64U * 1024U,
        .cache_bytes = 8U * 1024U,
    });

    const auto writer_count = 4U * opts.scale;
    const auto roots_per_writer = 8U;
    const auto rounds = 25U * opts.scale;
    const auto writes_per_root = rounds * roots_per_writer; // exact multiple: keeps the "last i per root" arithmetic below trivial
    std::atomic<bool> stop{false};
    std::atomic<std::uint64_t> reads_ok{0};

    // Every root each writer will ever touch, named up front so readers can pick among them.
    std::vector<std::string> all_roots;
    for (unsigned w = 0; w < writer_count; ++w)
        for (unsigned r = 0; r < roots_per_writer; ++r)
            all_roots.push_back("writer-" + std::to_string(w) + "-" + std::to_string(r));

    std::vector<std::thread> writers;
    writers.reserve(writer_count);
    for (unsigned w = 0; w < writer_count; ++w) {
        writers.emplace_back([&, w] {
            // 1KB per root x >=32 roots deliberately overruns the 8KB cache budget above,
            // so readers and the eviction path see real cache pressure, not just cache hits.
            const std::string payload(1024, static_cast<char>('a' + (w % 26)));
            for (unsigned i = 0; i < writes_per_root; ++i) {
                const auto& name = all_roots[w * roots_per_writer + (i % roots_per_writer)];
                auto tx = heap.begin();
                auto& root = tx.object_root(name);
                root["seq"] = static_cast<std::int64_t>(i);
                root["payload"] = payload;
                tx.commit();
            }
        });
    }

    std::thread checkpointer([&] {
        while (!stop.load(std::memory_order_relaxed)) {
            heap.checkpoint();
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    });

    std::vector<std::thread> readers;
    const auto reader_count = 2U * opts.scale;
    readers.reserve(reader_count);
    for (unsigned r = 0; r < reader_count; ++r) {
        readers.emplace_back([&, r] {
            std::mt19937_64 rng{opts.seed + r + 1};
            std::uniform_int_distribution<std::size_t> pick(0, all_roots.size() - 1);
            while (!stop.load(std::memory_order_relaxed)) {
                auto tx = heap.begin();
                const auto& value = tx.root(all_roots[pick(rng)]);
                // A root not yet written by its owner reads as null; anything else must be
                // the well-formed {seq, payload} object writers always commit atomically.
                if (!value.is_null()) {
                    ctx.check(value.as_object().contains("seq") && value.as_object().contains("payload"),
                               "reader observed a torn/partial root");
                    reads_ok.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }

    for (auto& writer : writers) writer.join();
    stop.store(true, std::memory_order_relaxed);
    for (auto& reader : readers) reader.join();
    checkpointer.join();

    heap.checkpoint();
    ctx.check(heap.check_integrity().ok, "heap failed integrity check after concurrent churn");

    auto verify = heap.begin();
    for (unsigned w = 0; w < writer_count; ++w) {
        for (unsigned r = 0; r < roots_per_writer; ++r) {
            const auto& name = all_roots[w * roots_per_writer + r];
            ctx.equal(verify.object_root(name).at("seq").as_integer(),
                       static_cast<std::int64_t>(writes_per_root - roots_per_writer + r),
                       "final value for a root does not match its writer's last commit");
        }
    }

    const auto cache = heap.cache();
    ctx.check(cache.bytes <= 8U * 1024U, "cache exceeded its configured byte budget");
    ctx.check(cache.evictions > 0, "working set never actually exceeded the cache budget -- scenario is not exercising eviction");
    std::cout << "  reads_ok=" << reads_ok.load()
              << " cache_evictions=" << cache.evictions
              << " cache_hits=" << cache.hits << " cache_misses=" << cache.misses << '\n';
}

} // namespace
