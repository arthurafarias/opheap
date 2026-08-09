#pragma once
#include "test_group.hpp"
#include "test_support.hpp"

#include <array>
#include <opheap/storage.hpp>

namespace opheap::testing {

inline static test_group storage_test{"storage", {
    {"append read truncate and flush", [](test_context& ctx) {
        auto directory = temporary_directory("storage");
        auto backend = make_default_storage_backend();
        auto file = backend->open_file(directory / "x.bin", true);
        std::array<std::byte, 4> bytes{std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}};
        ctx.equal(file->append(bytes), std::uint64_t{0});
        file->flush_all();
        std::array<std::byte, 4> read{};
        file->read_exact(0, read);
        ctx.check(read == bytes);
        file->truncate(2);
        ctx.equal(file->size(), std::uint64_t{2});
    }},
    {"atomic replace", [](test_context& ctx) {
        auto directory = temporary_directory("replace");
        auto backend = make_default_storage_backend();
        auto source = backend->open_file(directory / "tmp", true);
        std::array<std::byte, 1> byte{std::byte{7}};
        source->write_exact(0, byte);
        source->flush_all();
        source.reset();
        backend->atomic_replace(directory / "tmp", directory / "dst");
        ctx.check(backend->exists(directory / "dst"));
    }},
}};

} // namespace opheap::testing
