#pragma once
#include "test_group.hpp"
#include "test_support.hpp"

#include <array>
#include <opheap/storage.hpp>

namespace opheap::testing {

struct storage_test : public test_group {
    storage_test() : test_group("storage", {
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
    {"read_exact rejects reading past the end of the file", [](test_context& ctx) {
        auto directory = temporary_directory("storage-eof");
        auto backend = make_default_storage_backend();
        auto file = backend->open_file(directory / "empty.bin", true);
        std::array<std::byte, 1> out{};
        ctx.throws<corruption_error>([&] { file->read_exact(0, out); });
    }},
    {"opening a missing file without create_if_missing fails", [](test_context& ctx) {
        auto directory = temporary_directory("storage-missing");
        auto backend = make_default_storage_backend();
        ctx.throws<storage_error>([&] { (void)backend->open_file(directory / "missing.bin", false); });
    }},
    {"create_directories rejects a path blocked by an existing file", [](test_context& ctx) {
        auto directory = temporary_directory("storage-blocked-mkdir");
        auto backend = make_default_storage_backend();
        auto blocker = backend->open_file(directory / "blocker", true);
        blocker->write_exact(0, std::array<std::byte, 1>{std::byte{1}});
        blocker->flush_all();
        blocker.reset();
        ctx.throws<storage_error>([&] { backend->create_directories(directory / "blocker" / "nested"); });
    }},
    {"remove_file rejects a non-empty directory", [](test_context& ctx) {
        auto directory = temporary_directory("storage-remove-nonempty");
        auto backend = make_default_storage_backend();
        auto nested = directory / "nested";
        backend->create_directories(nested);
        auto file = backend->open_file(nested / "child", true);
        file->write_exact(0, std::array<std::byte, 1>{std::byte{1}});
        file->flush_all();
        file.reset();
        ctx.throws<storage_error>([&] { backend->remove_file(nested); });
    }},
    {"sync_directory rejects a directory that does not exist", [](test_context& ctx) {
        auto directory = temporary_directory("storage-sync-missing");
        auto backend = make_default_storage_backend();
        ctx.throws<storage_error>([&] { backend->sync_directory(directory / "missing"); });
    }},
    }) {}
};

inline static storage_test storage_test_instance;

} // namespace opheap::testing
