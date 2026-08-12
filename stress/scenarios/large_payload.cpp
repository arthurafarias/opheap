// Round-trips a single root holding a wide object (thousands of keys), a deep
// nesting chain, a large array (repeated push_back growth/reallocation), and a
// multi-megabyte string, twice in a row (to also exercise a large payload
// being superseded by another large payload) -- through commit, checkpoint
// and a fresh reopen. Every expected value is regenerated deterministically
// from (round, index) rather than kept around, so the checker's own memory
// footprint stays small even as the payload grows with --scale.
#include "../support.hpp"

#include <opheap/opheap.hpp>
#include <opheap/testing/temporary_directory.hpp>

#include <cstdint>
#include <string>

namespace {

struct shape {
    unsigned wide_count;
    unsigned depth;
    unsigned array_count;
    std::size_t elem_size;
    std::size_t blob_size;
};

shape make_shape(unsigned scale) {
    return shape{
        500U * scale,
        100U + 20U * std::min(scale, 20U),
        500U * scale,
        48U,
        512U * 1024U * std::min(scale, 8U),
    };
}

char blob_fill(unsigned round) { return static_cast<char>('A' + (round % 26)); }
char elem_fill(unsigned round, unsigned index) { return static_cast<char>('a' + ((round + index) % 26)); }

void populate(opheap::object& root, unsigned round, const shape& s) {
    auto& wide = root["wide"].as_object();
    wide.clear();
    for (unsigned i = 0; i < s.wide_count; ++i) {
        wide["k" + std::to_string(i)] = static_cast<std::int64_t>(round) * 1'000'000 + i;
    }

    opheap::value* cursor = &root["deep"];
    for (unsigned d = 0; d < s.depth; ++d) cursor = &(*cursor)["lvl"];
    *cursor = static_cast<std::int64_t>(round);

    auto& arr = root["arr"].as_array();
    arr.clear();
    for (unsigned i = 0; i < s.array_count; ++i) {
        arr.emplace_back(std::string(s.elem_size, elem_fill(round, i)));
    }

    root["blob"] = std::string(s.blob_size, blob_fill(round));
}

void verify(opheap::object& root, unsigned round, const shape& s, opheap::testing::test_context& ctx) {
    const auto& wide = root.at("wide").as_object();
    ctx.equal(wide.size(), std::size_t{s.wide_count}, "wide object lost keys across round-trip");
    for (unsigned i = 0; i < s.wide_count; i += std::max(1U, s.wide_count / 64U)) {
        ctx.equal(wide.at("k" + std::to_string(i)).as_integer(),
                   static_cast<std::int64_t>(round) * 1'000'000 + i,
                   "wide object value corrupted across round-trip");
    }

    const opheap::value* cursor = &root.at("deep");
    for (unsigned d = 0; d < s.depth; ++d) cursor = &cursor->at("lvl");
    ctx.equal(cursor->as_integer(), static_cast<std::int64_t>(round), "deep nesting value corrupted across round-trip");

    const auto& arr = root.at("arr").as_array();
    ctx.equal(arr.size(), std::size_t{s.array_count}, "array lost elements across round-trip");
    for (unsigned i = 0; i < s.array_count; i += std::max(1U, s.array_count / 64U)) {
        const auto expected = std::string(s.elem_size, elem_fill(round, i));
        ctx.check(arr.at(i).as_string().view() == expected, "array element corrupted across round-trip");
    }

    const auto& blob = root.at("blob").as_string();
    ctx.equal(blob.size(), s.blob_size, "blob size changed across round-trip");
    const auto expected_fill = blob_fill(round);
    ctx.check(blob.view().front() == expected_fill && blob.view().back() == expected_fill,
               "blob content corrupted across round-trip");
}

OPHEAP_STRESS_SCENARIO(large_payload) {
    const auto s = make_shape(opts.scale);
    auto directory = opheap::testing::temporary_directory("stress-large-payload");
    std::cout << "  wide=" << s.wide_count << " depth=" << s.depth
              << " array=" << s.array_count << " blob_bytes=" << s.blob_size << '\n';

    auto heap = opheap::heap::open({.path = directory});
    for (unsigned round = 0; round < 2; ++round) {
        auto tx = heap.begin();
        populate(tx.object_root(), round, s);
        tx.commit();

        auto read = heap.begin();
        verify(read.object_root(), round, s, ctx);
    }

    heap.checkpoint();
    {
        auto read = heap.begin();
        verify(read.object_root(), 1, s, ctx);
    }

    heap.close();
    auto reopened = opheap::heap::open({.path = directory});
    auto read = reopened.begin();
    verify(read.object_root(), 1, s, ctx);
    ctx.check(reopened.check_integrity().ok, "heap failed integrity check after large payload round-trip");
}

} // namespace
