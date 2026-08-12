#pragma once
#include "test_group.hpp"
#include "test_support.hpp"

#include <algorithm>
#include <opheap/detail/binary.hpp>
#include <opheap/detail/snapshot.hpp>

namespace opheap::testing {

namespace {

// Builds a raw snapshot header like detail::snapshot_store::save() does, but lets the
// caller set every field independently (including deliberately wrong ones), which
// save() never allows since it always derives them from a real snapshot_image.
std::vector<std::byte> build_snapshot_header(std::uint64_t magic, std::uint32_t format,
                                              sequence_number sequence, std::uint64_t root_count,
                                              std::uint64_t index_size, std::uint32_t index_crc,
                                              bool checksums) {
    detail::writer header_without_crc;
    header_without_crc.scalar(magic);
    header_without_crc.scalar(format);
    header_without_crc.scalar(sequence);
    header_without_crc.scalar<std::uint64_t>(root_count);
    header_without_crc.scalar<std::uint64_t>(index_size);
    header_without_crc.scalar(index_crc);
    detail::writer header;
    header.bytes(header_without_crc.data());
    header.scalar(checksums ? detail::crc32c(header_without_crc.data()) : 0U);
    return std::move(header).take();
}

void write_index_entry(detail::writer& index, std::string_view name, version_type version,
                        std::string_view type, std::uint64_t relative_offset,
                        std::uint64_t size, std::uint32_t checksum) {
    index.string(name);
    index.scalar(version);
    index.string(type);
    index.scalar(relative_offset);
    index.scalar(size);
    index.scalar(checksum);
}

void write_raw(std::shared_ptr<storage_backend>& backend, const std::filesystem::path& path,
               std::span<const std::byte> header, std::span<const std::byte> index = {}) {
    auto file = backend->open_file(path, true);
    [[maybe_unused]] auto header_offset = file->append(header);
    if (!index.empty()) { [[maybe_unused]] auto index_offset = file->append(index); }
    file->flush_all();
}

} // namespace

struct snapshot_test : public test_group {
    snapshot_test() : test_group("snapshot", {
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
    {"load rejects a file shorter than the header", [](test_context& ctx) {
        auto directory = temporary_directory("snapshot-truncated");
        auto backend = make_default_storage_backend();
        std::array<std::byte, 10> short_file{};
        write_raw(backend, directory / "heap.snapshot", short_file);
        detail::snapshot_store snapshot{directory / "heap.snapshot", backend, durability_mode::strict, true};
        ctx.throws<corruption_error>([&] { (void)snapshot.load(); });
    }},
    {"load rejects a bad magic and an unsupported format", [](test_context& ctx) {
        auto backend = make_default_storage_backend();
        auto directory = temporary_directory("snapshot-magic");
        auto bad_magic = build_snapshot_header(0xdeadbeefULL, detail::snapshot_format, 0, 0, 0, 0, true);
        write_raw(backend, directory / "heap.snapshot", bad_magic);
        detail::snapshot_store magic_snapshot{directory / "heap.snapshot", backend, durability_mode::strict, true};
        ctx.throws<corruption_error>([&] { (void)magic_snapshot.load(); });

        auto other_directory = temporary_directory("snapshot-format");
        auto bad_format = build_snapshot_header(detail::snapshot_magic, 999, 0, 0, 0, 0, true);
        write_raw(backend, other_directory / "heap.snapshot", bad_format);
        detail::snapshot_store format_snapshot{other_directory / "heap.snapshot", backend, durability_mode::strict, true};
        ctx.throws<corruption_error>([&] { (void)format_snapshot.load(); });
    }},
    {"load rejects a header whose checksum does not match", [](test_context& ctx) {
        auto directory = temporary_directory("snapshot-header-checksum");
        auto backend = make_default_storage_backend();
        // Built with checksums=false so the stored header checksum is 0, while the
        // store below verifies with checksums=true - guaranteeing a mismatch.
        auto header = build_snapshot_header(detail::snapshot_magic, detail::snapshot_format, 0, 0, 0, 0, false);
        write_raw(backend, directory / "heap.snapshot", header);
        detail::snapshot_store snapshot{directory / "heap.snapshot", backend, durability_mode::strict, true};
        ctx.throws<corruption_error>([&] { (void)snapshot.load(); });
    }},
    {"load rejects an implausible root count", [](test_context& ctx) {
        auto directory = temporary_directory("snapshot-root-count");
        auto backend = make_default_storage_backend();
        auto header = build_snapshot_header(detail::snapshot_magic, detail::snapshot_format, 0, 10'000'001ULL, 0, 0, true);
        write_raw(backend, directory / "heap.snapshot", header);
        detail::snapshot_store snapshot{directory / "heap.snapshot", backend, durability_mode::strict, true};
        ctx.throws<corruption_error>([&] { (void)snapshot.load(); });
    }},
    {"load rejects an index size that exceeds the file", [](test_context& ctx) {
        auto directory = temporary_directory("snapshot-index-exceeds");
        auto backend = make_default_storage_backend();
        auto header = build_snapshot_header(detail::snapshot_magic, detail::snapshot_format, 0, 0, 1000, 0, true);
        write_raw(backend, directory / "heap.snapshot", header);
        detail::snapshot_store snapshot{directory / "heap.snapshot", backend, durability_mode::strict, true};
        ctx.throws<corruption_error>([&] { (void)snapshot.load(); });
    }},
    {"load rejects an index whose checksum does not match", [](test_context& ctx) {
        auto directory = temporary_directory("snapshot-index-checksum");
        auto backend = make_default_storage_backend();
        std::array<std::byte, 4> index{std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}};
        auto header = build_snapshot_header(detail::snapshot_magic, detail::snapshot_format, 0, 0, index.size(), 0, true);
        write_raw(backend, directory / "heap.snapshot", header, index);
        detail::snapshot_store snapshot{directory / "heap.snapshot", backend, durability_mode::strict, true};
        ctx.throws<corruption_error>([&] { (void)snapshot.load(); });
    }},
    {"load rejects a payload locator that exceeds the file", [](test_context& ctx) {
        auto directory = temporary_directory("snapshot-locator-exceeds");
        auto backend = make_default_storage_backend();
        detail::writer index_writer;
        write_index_entry(index_writer, "root", 1, "opheap.value.v1", 0, 1'000'000, 0);
        const auto index_crc = detail::crc32c(index_writer.data());
        auto header = build_snapshot_header(detail::snapshot_magic, detail::snapshot_format, 0, 1,
                                             index_writer.data().size(), index_crc, true);
        write_raw(backend, directory / "heap.snapshot", header, index_writer.data());
        detail::snapshot_store snapshot{directory / "heap.snapshot", backend, durability_mode::strict, true};
        ctx.throws<corruption_error>([&] { (void)snapshot.load(); });
    }},
    {"load rejects a duplicate root name in the index", [](test_context& ctx) {
        auto directory = temporary_directory("snapshot-duplicate-root");
        auto backend = make_default_storage_backend();
        detail::writer index_writer;
        write_index_entry(index_writer, "root", 1, "opheap.value.v1", 0, 0, 0);
        write_index_entry(index_writer, "root", 2, "opheap.value.v1", 0, 0, 0);
        const auto index_crc = detail::crc32c(index_writer.data());
        auto header = build_snapshot_header(detail::snapshot_magic, detail::snapshot_format, 0, 2,
                                             index_writer.data().size(), index_crc, true);
        write_raw(backend, directory / "heap.snapshot", header, index_writer.data());
        detail::snapshot_store snapshot{directory / "heap.snapshot", backend, durability_mode::strict, true};
        ctx.throws<corruption_error>([&] { (void)snapshot.load(); });
    }},
    {"load rejects trailing bytes past the declared index entries", [](test_context& ctx) {
        auto directory = temporary_directory("snapshot-index-trailing");
        auto backend = make_default_storage_backend();
        std::array<std::byte, 4> index{std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}};
        const auto index_crc = detail::crc32c(index);
        auto header = build_snapshot_header(detail::snapshot_magic, detail::snapshot_format, 0, 0,
                                             index.size(), index_crc, true);
        write_raw(backend, directory / "heap.snapshot", header, index);
        detail::snapshot_store snapshot{directory / "heap.snapshot", backend, durability_mode::strict, true};
        ctx.throws<corruption_error>([&] { (void)snapshot.load(); });
    }},
    {"read and load reject a locator that does not belong to the snapshot", [](test_context& ctx) {
        auto directory = temporary_directory("snapshot-locator-kind");
        auto backend = make_default_storage_backend();
        detail::snapshot_store snapshot{directory / "heap.snapshot", backend, durability_mode::strict, true};
        std::array<std::byte, 1> out{};
        ctx.throws<storage_error>([&] { snapshot.read(detail::loc{detail::source::journal, 0, 1, 0}, 0, out); });
    }},
    {"read rejects an offset/size pair that exceeds the locator", [](test_context& ctx) {
        auto directory = temporary_directory("snapshot-locator-bounds");
        auto backend = make_default_storage_backend();
        detail::snapshot_store snapshot{directory / "heap.snapshot", backend, durability_mode::strict, true};
        std::array<std::byte, 4> out{};
        ctx.throws<corruption_error>([&] { snapshot.read(detail::loc{detail::source::snapshot, 0, 4, 0}, 2, out); });
    }},
    {"load rejects a payload whose checksum does not match its locator", [](test_context& ctx) {
        auto directory = temporary_directory("snapshot-payload-checksum");
        auto backend = make_default_storage_backend();
        detail::snapshot_store snapshot{directory / "heap.snapshot", backend, durability_mode::strict, true};
        const std::vector<std::byte> payload{std::byte{5}};
        detail::snapshot_image image;
        image.roots.emplace("root", detail::root_record{
            1, "opheap.value.v1",
            detail::loc{detail::source::journal, 0, payload.size(), detail::crc32c(payload)}});
        auto written = snapshot.save(image, [&](const detail::root_record&, std::uint64_t offset,
                                                std::span<std::byte> out) {
            std::copy_n(payload.begin() + static_cast<std::ptrdiff_t>(offset), out.size(), out.begin());
        });
        auto tampered = written.roots.at("root").payload;
        tampered.checksum ^= 0xffffffffU;
        ctx.throws<corruption_error>([&] { (void)snapshot.load(tampered); });
    }},
    {"ordered_roots rejects a payload size that would overflow the total offset", [](test_context& ctx) {
        detail::snapshot_image image;
        image.roots.emplace("a", detail::root_record{
            1, "opheap.value.v1", detail::loc{detail::source::snapshot, 0, std::numeric_limits<std::uint64_t>::max(), 0}});
        image.roots.emplace("b", detail::root_record{
            1, "opheap.value.v1", detail::loc{detail::source::snapshot, 0, 1, 0}});
        ctx.throws<storage_error>([&] { (void)detail::ordered_roots(image); });
    }},
    }) {}
};

inline static snapshot_test snapshot_test_instance;

} // namespace opheap::testing
