// Generalizes fault_injection_test.hpp's three fixed-point crash scenarios
// into many randomized trials: instead of injecting each fault at a single
// hand-picked moment, grow the journal by a random number of good commits
// first, THEN inject the fault, for many (trial, fault-kind, history-length)
// combinations. After every trial the poisoned heap is dropped and a fresh,
// fault-free heap is opened from the same directory: recovery must always
// pass check_integrity() and land on a well-defined value -- never a torn one.
//
// The three fault kinds are not equivalent here: journal::append() writes a
// record's bytes to the file and only *then* calls flush_data(), so failing
// the flush (not the write) leaves a complete, checksum-valid record already
// on disk -- commit() still throws (durability is unconfirmed, so the API
// must not report success) and the heap poisons itself, but recovery will
// see that record as committed anyway. partial_append and fail_replace never
// leave a complete record/replacement behind, so their faulted write is
// never observed after recovery. Each branch below encodes which of those
// two outcomes it expects.
#include "../support.hpp"

#include <opheap/opheap.hpp>
#include <opheap/testing/fault_backend.hpp>
#include <opheap/testing/temporary_directory.hpp>

#include <cstdint>
#include <memory>
#include <random>
#include <string>

namespace {

using opheap::testing::fault_backend;
using opheap::testing::fault_plan;

enum class fault_kind { partial_append, fail_flush, fail_replace };

void run_trial(const opheap::stress::options&, opheap::testing::test_context& ctx,
                unsigned trial_index, fault_kind kind, unsigned good_commits) {
    auto directory = opheap::testing::temporary_directory(
        "stress-fault-fuzz-" + std::to_string(trial_index));
    auto plan = std::make_shared<fault_plan>();
    auto backend = std::make_shared<fault_backend>(opheap::make_default_storage_backend(), plan);

    // How many commits must be visible after recovery: good_commits, plus one more
    // if the faulted commit's bytes land on disk despite the fault (see above).
    unsigned visible_commits = good_commits;

    {
        auto heap = opheap::heap::open({.path = directory}, backend);
        for (unsigned i = 0; i < good_commits; ++i) {
            auto tx = heap.begin();
            tx.object_root()["n"] = static_cast<std::int64_t>(i);
            tx.commit();
        }

        switch (kind) {
        case fault_kind::partial_append:
            plan->partial_append_once = true;
            {
                auto tx = heap.begin();
                tx.object_root()["n"] = static_cast<std::int64_t>(good_commits);
                ctx.throws<opheap::storage_error>([&] { tx.commit(); }, "partial append fault was not surfaced");
            }
            break;
        case fault_kind::fail_flush:
            plan->fail_flush_data_once = true;
            {
                auto tx = heap.begin();
                tx.object_root()["n"] = static_cast<std::int64_t>(good_commits);
                ctx.throws<opheap::storage_error>([&] { tx.commit(); }, "flush fault was not surfaced");
            }
            visible_commits = good_commits + 1;
            // A durability failure poisons the whole heap: nothing further may commit.
            {
                auto next = heap.begin();
                next.object_root("other")["n"] = 1;
                ctx.throws<opheap::storage_error>([&] { next.commit(); }, "heap was not poisoned after flush failure");
            }
            break;
        case fault_kind::fail_replace:
            plan->fail_replace_once = true;
            ctx.throws<opheap::storage_error>([&] { heap.checkpoint(); }, "atomic replace fault was not surfaced");
            break;
        }
    }

    auto recovered = opheap::heap::open({.path = directory});
    const auto report = recovered.check_integrity();
    ctx.check(report.ok, "recovery failed integrity check after injected fault: " + report.message);

    auto read = recovered.begin();
    if (visible_commits == 0) {
        ctx.check(read.root().is_null(), "root should be empty when the fault hit before any commit");
    } else {
        ctx.equal(read.object_root().at("n").as_integer(), static_cast<std::int64_t>(visible_commits - 1),
                   "recovered value does not match the expected last visible commit");
    }
}

OPHEAP_STRESS_SCENARIO(fault_fuzz) {
    std::mt19937_64 rng{opts.seed};
    const auto trials = 20U * opts.scale;
    std::uniform_int_distribution<unsigned> history(0, 40);
    std::uniform_int_distribution<int> kind_pick(0, 2);

    for (unsigned trial = 0; trial < trials; ++trial) {
        const auto good_commits = history(rng);
        const auto kind = static_cast<fault_kind>(kind_pick(rng));
        run_trial(opts, ctx, trial, kind, good_commits);
    }
    std::cout << "  trials=" << trials << '\n';
}

} // namespace
