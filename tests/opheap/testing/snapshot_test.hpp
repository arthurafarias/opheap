#pragma once
#include "test_group.hpp"
#include "test_support.hpp"

#include <algorithm>
#include <opheap/detail/binary.hpp>
#include <opheap/detail/snapshot.hpp>

namespace opheap::testing {

inline static test_group snapshot_test{"snapshot", {
    {"save and load locator index without materializing payload", [](test_context& ctx) {
        auto directory = temporary_directory("snapshot");
        auto backend = make_default_storage_backend();
        detail::snapshot_store snapshot{directory / "heap.snapshot", backend, durability_mode::strict, true};
        const std::vector<std::byte> payload{std::byte{9}};
        detail::snapshot_image image;
        image.sequence = 19;
        image.roots.emplace("root", detail::root_record{
            3,
            "opheap.value.v1",
            detail::loc{detail::source::journal, 0, payload.size(), detail::crc32c(payload)}});
        auto written = snapshot.save(image, [&](const detail::root_record&, std::uint64_t offset,
                                                std::span<std::byte> out) {
            std::copy_n(payload.begin() + static_cast<std::ptrdiff_t>(offset), out.size(), out.begin());
        });
        ctx.equal(written.roots.at("root").payload.kind, detail::source::snapshot);
        auto loaded = snapshot.load();
        ctx.equal(loaded.sequence, sequence_number{19});
        ctx.equal(loaded.roots.at("root").version, version_type{3});
        ctx.equal(loaded.roots.at("root").payload.size, std::uint64_t{1});
        auto bytes = snapshot.load(loaded.roots.at("root").payload);
        ctx.equal(bytes.size(), std::size_t{1});
        ctx.check(bytes.front() == std::byte{9});
    }},
}};

} // namespace opheap::testing
