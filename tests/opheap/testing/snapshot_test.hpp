#pragma once
#include "test_group.hpp"
#include "test_support.hpp"

#include <opheap/detail/snapshot.hpp>

namespace opheap::testing {

inline static test_group snapshot_test{"snapshot", {
    {"save and load image", [](test_context& ctx) {
        auto directory = temporary_directory("snapshot");
        auto backend = make_default_storage_backend();
        detail::snapshot_store snapshot{directory / "heap.snapshot", backend, durability_mode::strict, true};
        detail::snapshot_image image;
        image.sequence = 19;
        image.roots.emplace("root", detail::root_record{3, "opheap.value.v1", {std::byte{9}}});
        snapshot.save(image);
        auto loaded = snapshot.load();
        ctx.equal(loaded.sequence, sequence_number{19});
        ctx.equal(loaded.roots.at("root").version, version_type{3});
    }},
}};

} // namespace opheap::testing
