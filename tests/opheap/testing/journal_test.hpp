#pragma once
#include "test_group.hpp"
#include "test_support.hpp"

#include <opheap/detail/journal.hpp>

namespace opheap::testing {

inline static test_group journal_test{"journal", {
    {"replays only committed root replacement", [](test_context& ctx) {
        auto directory = temporary_directory("journal");
        auto backend = make_default_storage_backend();
        detail::journal wal{directory / "heap.wal", backend, durability_mode::strict, true};
        detail::root_update update{"root", 0, 1, "opheap.value.v1", {std::byte{1}, std::byte{2}}};
        [[maybe_unused]] auto written = wal.append(1, {update});
        auto recovered = wal.recover({}, 0, true);
        ctx.equal(recovered.roots.at("root").version, version_type{1});
        ctx.equal(recovered.roots.at("root").payload.size, std::uint64_t{2});
        auto payload = wal.load(recovered.roots.at("root").payload);
        ctx.equal(payload.size(), std::size_t{2});
        ctx.check(payload[0] == std::byte{1} && payload[1] == std::byte{2});
    }},
    {"torn commit does not publish transaction", [](test_context& ctx) {
        auto directory = temporary_directory("journal-torn-commit");
        auto backend = make_default_storage_backend();
        detail::journal wal{directory / "heap.wal", backend, durability_mode::strict, true};
        detail::root_update update{"root", 0, 1, "opheap.value.v1", {std::byte{3}}};
        [[maybe_unused]] auto written = wal.append(1, {update});
        auto file = backend->open_file(directory / "heap.wal", false);
        const auto size = file->size();
        file->truncate(size - 8);
        file->flush_all();
        file.reset();
        auto recovered = wal.recover({}, 0, true);
        ctx.check(!recovered.roots.contains("root"));
    }},
    {"torn tail is discarded and truncated", [](test_context& ctx) {
        auto directory = temporary_directory("journal-tail");
        auto backend = make_default_storage_backend();
        detail::journal wal{directory / "heap.wal", backend, durability_mode::strict, true};
        detail::root_update update{"root", 0, 1, "opheap.value.v1", {std::byte{1}}};
        [[maybe_unused]] auto written = wal.append(1, {update});
        auto file = backend->open_file(directory / "heap.wal", false);
        const auto intact = file->size();
        std::array<std::byte, 5> garbage{std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}, std::byte{5}};
        [[maybe_unused]] const auto garbage_offset = file->append(garbage);
        file.reset();
        auto recovered = wal.recover({}, 0, true);
        ctx.equal(recovered.roots.at("root").version, version_type{1});
        auto verify = backend->open_file(directory / "heap.wal", false);
        ctx.equal(verify->size(), intact);
    }},
}};

} // namespace opheap::testing
