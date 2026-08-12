#pragma once
#include "test_group.hpp"
#include "test_support.hpp"

#include <opheap/detail/journal.hpp>

namespace opheap::testing {

namespace {

// Builds a raw journal record frame like detail::make_record(), but lets the caller lie
// about the declared payload size independently of how many payload bytes actually
// follow it - detail::make_record() always keeps those in sync, so corrupting that
// relationship (to exercise the "length mismatch" / "trailing bytes" checks) requires
// building the frame by hand instead.
std::vector<std::byte> make_frame_with_declared_size(detail::record_type type, sequence_number sequence,
                                                       transaction_id tx, std::uint64_t declared_payload_size,
                                                       std::span<const std::byte> payload_bytes, bool checksums) {
    detail::writer body;
    body.scalar(detail::record_format);
    body.scalar(type);
    body.scalar(sequence);
    body.scalar(tx);
    body.scalar<std::uint64_t>(declared_payload_size);
    body.bytes(payload_bytes);
    const auto checksum = checksums ? detail::crc32c(body.data()) : 0U;

    detail::writer frame;
    frame.scalar(detail::record_magic);
    frame.scalar<std::uint64_t>(body.data().size() + sizeof(std::uint32_t));
    frame.bytes(body.data());
    frame.scalar(checksum);
    return std::move(frame).take();
}

void append_raw(std::shared_ptr<storage_backend>& backend, const std::filesystem::path& path,
                 std::span<const std::byte> bytes) {
    auto file = backend->open_file(path, true);
    [[maybe_unused]] auto offset = file->append(bytes);
    file->flush_all();
}

} // namespace

struct journal_test : public test_group {
    journal_test() : test_group("journal", {
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
    {"recover rejects a corrupted record prefix", [](test_context& ctx) {
        auto directory = temporary_directory("journal-magic");
        auto backend = make_default_storage_backend();
        detail::writer bad_prefix;
        bad_prefix.scalar<std::uint32_t>(0xdeadbeefU);
        bad_prefix.scalar<std::uint64_t>(4);
        append_raw(backend, directory / "heap.wal", bad_prefix.data());
        detail::journal wal{directory / "heap.wal", backend, durability_mode::strict, true};
        ctx.throws<corruption_error>([&] { (void)wal.recover({}, 0, true); });
    }},
    {"recover rejects an implausible record length", [](test_context& ctx) {
        auto directory = temporary_directory("journal-length");
        auto backend = make_default_storage_backend();
        detail::writer bad_length;
        bad_length.scalar(detail::record_magic);
        bad_length.scalar<std::uint64_t>(2);
        append_raw(backend, directory / "heap.wal", bad_length.data());
        detail::journal wal{directory / "heap.wal", backend, durability_mode::strict, true};
        ctx.throws<corruption_error>([&] { (void)wal.recover({}, 0, true); });
    }},
    {"recover rejects a record whose checksum does not match its body", [](test_context& ctx) {
        auto directory = temporary_directory("journal-checksum");
        auto backend = make_default_storage_backend();
        detail::journal wal{directory / "heap.wal", backend, durability_mode::strict, true};
        detail::root_update update{"root", 0, 1, "opheap.value.v1", {std::byte{1}}};
        [[maybe_unused]] auto written = wal.append(1, {update});
        auto file = backend->open_file(directory / "heap.wal", false);
        const auto size = file->size();
        std::array<std::byte, 1> last{};
        file->read_exact(size - 1, last);
        last[0] ^= std::byte{0xff};
        file->write_exact(size - 1, last);
        file->flush_all();
        file.reset();
        ctx.throws<corruption_error>([&] { (void)wal.recover({}, 0, true); });
    }},
    {"recover rejects a payload length that exceeds the record", [](test_context& ctx) {
        auto directory = temporary_directory("journal-payload-length");
        auto backend = make_default_storage_backend();
        auto frame = make_frame_with_declared_size(detail::record_type::begin, 1, 1, 100, {}, true);
        append_raw(backend, directory / "heap.wal", frame);
        detail::journal wal{directory / "heap.wal", backend, durability_mode::strict, true};
        ctx.throws<corruption_error>([&] { (void)wal.recover({}, 0, true); });
    }},
    {"recover rejects a record with trailing bytes past its declared payload", [](test_context& ctx) {
        auto directory = temporary_directory("journal-trailing");
        auto backend = make_default_storage_backend();
        std::array<std::byte, 1> extra{std::byte{7}};
        auto frame = make_frame_with_declared_size(detail::record_type::begin, 1, 1, 0, extra, true);
        append_raw(backend, directory / "heap.wal", frame);
        detail::journal wal{directory / "heap.wal", backend, durability_mode::strict, true};
        ctx.throws<corruption_error>([&] { (void)wal.recover({}, 0, true); });
    }},
    {"recover rejects a non-increasing sequence number", [](test_context& ctx) {
        auto directory = temporary_directory("journal-sequence");
        auto backend = make_default_storage_backend();
        auto first = detail::make_record(detail::record_type::begin, 5, 1, {}, true);
        auto second = detail::make_record(detail::record_type::begin, 5, 2, {}, true);
        append_raw(backend, directory / "heap.wal", first);
        append_raw(backend, directory / "heap.wal", second);
        detail::journal wal{directory / "heap.wal", backend, durability_mode::strict, true};
        ctx.throws<corruption_error>([&] { (void)wal.recover({}, 0, true); });
    }},
    {"recover rejects a root update record without a preceding BEGIN", [](test_context& ctx) {
        auto directory = temporary_directory("journal-update-no-begin");
        auto backend = make_default_storage_backend();
        auto frame = detail::make_record(detail::record_type::root_update, 1, 1, {}, true);
        append_raw(backend, directory / "heap.wal", frame);
        detail::journal wal{directory / "heap.wal", backend, durability_mode::strict, true};
        ctx.throws<corruption_error>([&] { (void)wal.recover({}, 0, true); });
    }},
    {"recover rejects a COMMIT record without a preceding BEGIN", [](test_context& ctx) {
        auto directory = temporary_directory("journal-commit-no-begin");
        auto backend = make_default_storage_backend();
        auto frame = detail::make_record(detail::record_type::commit, 1, 1, {}, true);
        append_raw(backend, directory / "heap.wal", frame);
        detail::journal wal{directory / "heap.wal", backend, durability_mode::strict, true};
        ctx.throws<corruption_error>([&] { (void)wal.recover({}, 0, true); });
    }},
    {"recover rejects a COMMIT whose update count does not match", [](test_context& ctx) {
        auto directory = temporary_directory("journal-commit-count");
        auto backend = make_default_storage_backend();
        auto begin = detail::make_record(detail::record_type::begin, 1, 1, {}, true);
        detail::writer commit_payload;
        commit_payload.scalar<std::uint64_t>(5);
        auto commit = detail::make_record(detail::record_type::commit, 2, 1, commit_payload.data(), true);
        append_raw(backend, directory / "heap.wal", begin);
        append_raw(backend, directory / "heap.wal", commit);
        detail::journal wal{directory / "heap.wal", backend, durability_mode::strict, true};
        ctx.throws<corruption_error>([&] { (void)wal.recover({}, 0, true); });
    }},
    {"recover rejects a version chain that skips the expected version", [](test_context& ctx) {
        auto directory = temporary_directory("journal-version-chain");
        auto backend = make_default_storage_backend();
        detail::root_update update{"root", 5, 1, "opheap.value.v1", {std::byte{1}}};
        auto encoded = detail::encode_update(update);
        auto begin = detail::make_record(detail::record_type::begin, 1, 1, {}, true);
        auto root_update_record = detail::make_record(detail::record_type::root_update, 2, 1, encoded.bytes, true);
        detail::writer commit_payload;
        commit_payload.scalar<std::uint64_t>(1);
        auto commit = detail::make_record(detail::record_type::commit, 3, 1, commit_payload.data(), true);
        append_raw(backend, directory / "heap.wal", begin);
        append_raw(backend, directory / "heap.wal", root_update_record);
        append_raw(backend, directory / "heap.wal", commit);
        detail::journal wal{directory / "heap.wal", backend, durability_mode::strict, true};
        ctx.throws<corruption_error>([&] { (void)wal.recover({}, 0, true); });
    }},
    {"recover rejects an unknown record type", [](test_context& ctx) {
        auto directory = temporary_directory("journal-unknown-type");
        auto backend = make_default_storage_backend();
        auto frame = detail::make_record(static_cast<detail::record_type>(99), 1, 1, {}, true);
        append_raw(backend, directory / "heap.wal", frame);
        detail::journal wal{directory / "heap.wal", backend, durability_mode::strict, true};
        ctx.throws<corruption_error>([&] { (void)wal.recover({}, 0, true); });
    }},
    {"read and load reject a locator that does not belong to the journal", [](test_context& ctx) {
        auto directory = temporary_directory("journal-locator");
        auto backend = make_default_storage_backend();
        detail::journal wal{directory / "heap.wal", backend, durability_mode::strict, true};
        std::array<std::byte, 1> out{};
        ctx.throws<storage_error>([&] { wal.read(detail::loc{detail::source::snapshot, 0, 1, 0}, 0, out); });
    }},
    {"read rejects an offset/size pair that exceeds the locator", [](test_context& ctx) {
        auto directory = temporary_directory("journal-locator-bounds");
        auto backend = make_default_storage_backend();
        detail::journal wal{directory / "heap.wal", backend, durability_mode::strict, true};
        std::array<std::byte, 4> out{};
        ctx.throws<corruption_error>([&] { wal.read(detail::loc{detail::source::journal, 0, 4, 0}, 2, out); });
    }},
    {"load rejects a payload whose checksum does not match its locator", [](test_context& ctx) {
        auto directory = temporary_directory("journal-payload-checksum");
        auto backend = make_default_storage_backend();
        detail::journal wal{directory / "heap.wal", backend, durability_mode::strict, true};
        detail::root_update update{"root", 0, 1, "opheap.value.v1", {std::byte{9}}};
        auto written = wal.append(1, {update});
        auto tampered = written.front();
        tampered.checksum ^= 0xffffffffU;
        ctx.throws<corruption_error>([&] { (void)wal.load(tampered); });
    }},
    {"decode_update rejects a payload length that exceeds the record", [](test_context& ctx) {
        detail::writer w;
        w.string("root");
        w.scalar<version_type>(0);
        w.scalar<version_type>(1);
        w.string("opheap.value.v1");
        w.scalar<std::uint64_t>(100);
        ctx.throws<corruption_error>([&] { (void)detail::decode_update(w.data(), 0); });
    }},
    {"decode_update rejects trailing bytes past the declared payload", [](test_context& ctx) {
        detail::writer w;
        w.string("root");
        w.scalar<version_type>(0);
        w.scalar<version_type>(1);
        w.string("opheap.value.v1");
        w.scalar<std::uint64_t>(0);
        w.bytes(std::array<std::byte, 1>{std::byte{9}});
        ctx.throws<corruption_error>([&] { (void)detail::decode_update(w.data(), 0); });
    }},
    }) {}
};

inline static journal_test journal_test_instance;

} // namespace opheap::testing
