// Property-based fuzzer: drives a long random sequence of mutations (set
// scalar, replace with nested object/array, erase key, push_back, abort,
// checkpoint, full close+reopen) against a handful of persistent roots, while
// mirroring every *committed* mutation into a plain in-memory opheap::value
// tree that never touches storage. After every commit the heap's freshly
// re-read root must structurally equal the mirror; an aborted transaction
// must leave both completely untouched. This is the one scenario not
// hand-enumerating scenarios: it is only as good as the operation mix below,
// but it will happily wander into interactions the other scenarios don't.
#include "../support.hpp"

#include <opheap/opheap.hpp>
#include <opheap/testing/temporary_directory.hpp>

#include <random>
#include <string>
#include <vector>

namespace {

bool deep_equal(const opheap::value& a, const opheap::value& b) {
    if (a.storage().index() != b.storage().index()) return false;
    if (a.is_null()) return true;
    if (a.is_bool()) return a.as_bool() == b.as_bool();
    if (a.is_integer()) return a.as_integer() == b.as_integer();
    if (a.is_number()) return a.as_number() == b.as_number();
    if (a.is_string()) return a.as_string().view() == b.as_string().view();
    if (a.is_array()) {
        const auto& x = a.as_array();
        const auto& y = b.as_array();
        if (x.size() != y.size()) return false;
        for (std::size_t i = 0; i < x.size(); ++i) if (!deep_equal(x.at(i), y.at(i))) return false;
        return true;
    }
    const auto& x = a.as_object();
    const auto& y = b.as_object();
    if (x.size() != y.size()) return false;
    for (const auto& [key, value] : x) {
        auto found = y.find(key);
        if (found == y.end() || !deep_equal(value, found->second)) return false;
    }
    return true;
}

// Walks/creates a path of "field0"/"field1"/... object keys inside a value,
// bounded to a modest depth so the fuzzer occasionally revisits shallow
// siblings instead of only ever growing deeper.
opheap::value& walk(opheap::value& root, const std::vector<std::string>& path) {
    opheap::value* cursor = &root;
    for (const auto& key : path) cursor = &cursor->as_object()[key];
    return *cursor;
}

std::vector<std::string> random_path(std::mt19937_64& rng, unsigned max_depth) {
    std::uniform_int_distribution<unsigned> depth_pick(0, max_depth);
    std::uniform_int_distribution<unsigned> field_pick(0, 3);
    std::vector<std::string> path;
    const auto depth = depth_pick(rng);
    for (unsigned d = 0; d < depth; ++d) path.push_back("field" + std::to_string(field_pick(rng)));
    return path;
}

opheap::value random_scalar(std::mt19937_64& rng) {
    std::uniform_int_distribution<int> kind(0, 3);
    switch (kind(rng)) {
    case 0: return opheap::value{std::uniform_int_distribution<std::int64_t>{-1'000'000, 1'000'000}(rng)};
    case 1: return opheap::value{std::uniform_real_distribution<double>{-1e6, 1e6}(rng)};
    case 2: return opheap::value{static_cast<bool>(rng() % 2)};
    default: {
        const auto length = std::uniform_int_distribution<std::size_t>{0, 24}(rng);
        std::string text(length, ' ');
        for (auto& c : text) c = static_cast<char>('a' + rng() % 26);
        return opheap::value{std::string_view{text}};
    }
    }
}

OPHEAP_STRESS_SCENARIO(model_fuzz) {
    auto directory = opheap::testing::temporary_directory("stress-model-fuzz");
    auto heap = std::make_unique<opheap::heap>(opheap::heap::open({.path = directory}));

    const std::vector<std::string> root_names{"a", "b", "c"};
    std::vector<opheap::value> model(root_names.size());

    std::mt19937_64 rng{opts.seed};
    const auto iterations = 2'000U * opts.scale;
    std::uniform_int_distribution<std::size_t> root_pick(0, root_names.size() - 1);
    std::uniform_int_distribution<int> op_pick(0, 5);
    unsigned reopen_countdown = 200;

    for (unsigned i = 0; i < iterations; ++i) {
        const auto slot = root_pick(rng);
        auto tx = heap->begin();
        auto& live = tx.root(root_names[slot]);
        auto trial_model = model[slot]; // tentative: only committed on tx.commit() below

        switch (op_pick(rng)) {
        case 0: { // set scalar at a random path
            auto path = random_path(rng, 3);
            auto scalar = random_scalar(rng);
            walk(live, path) = scalar;
            walk(trial_model, path) = scalar;
            break;
        }
        case 1: { // erase a key from an existing object, if one exists at a random path
            auto path = random_path(rng, 2);
            auto& node = walk(live, path);
            auto& model_node = walk(trial_model, path);
            if (node.is_object() && !node.as_object().empty()) {
                const auto key = node.as_object().begin()->first;
                node.as_object().erase(key);
                model_node.as_object().erase(key);
            }
            break;
        }
        case 2: { // push_back onto an array at a random path
            auto path = random_path(rng, 2);
            auto scalar = random_scalar(rng);
            walk(live, path).as_array().emplace_back(scalar);
            walk(trial_model, path).as_array().emplace_back(std::move(scalar));
            break;
        }
        default: { // plain scalar write is the common case, weighted via the default branch
            auto path = random_path(rng, 3);
            auto scalar = random_scalar(rng);
            walk(live, path) = scalar;
            walk(trial_model, path) = scalar;
            break;
        }
        }

        const auto should_abort = (i % 7 == 0);
        if (should_abort) {
            tx.abort();
        } else {
            tx.commit();
            model[slot] = std::move(trial_model);
        }

        auto read = heap->begin();
        ctx.check(deep_equal(read.root(root_names[slot]), model[slot]),
                   "heap root diverged from reference model after " + std::string(should_abort ? "abort" : "commit"));

        if (--reopen_countdown == 0) {
            reopen_countdown = 200;
            heap->checkpoint();
            heap = std::make_unique<opheap::heap>(opheap::heap::open({.path = directory}));
            for (std::size_t r = 0; r < root_names.size(); ++r) {
                auto verify = heap->begin();
                ctx.check(deep_equal(verify.root(root_names[r]), model[r]),
                           "heap root diverged from reference model across a checkpoint+reopen");
            }
        }
    }

    ctx.check(heap->check_integrity().ok, "heap failed integrity check after model-fuzzing");
    std::cout << "  iterations=" << iterations << '\n';
}

} // namespace
