#include "opheap/detail/snapshot.hpp"

#include "opheap/detail/binary.hpp"

#include <array>
#include <span>

namespace opheap::detail {
namespace {
constexpr std::uint64_t snapshot_magic = 0x3150414e53504fULL; // "OPSNAP1" little-endian-ish marker
constexpr std::uint32_t snapshot_format = 1;
}

snapshot_image snapshot_store::load() const {
    if (!storage_->exists(path_)) return {};
    auto file = storage_->open_file(path_, false);
    if (file->size() == 0) return {};
    if (file->size() > (1ULL << 42U)) throw corruption_error("snapshot is implausibly large");
    std::vector<std::byte> bytes(static_cast<std::size_t>(file->size()));
    file->read_exact(0, bytes);
    if (bytes.size() < sizeof(std::uint32_t)) throw corruption_error("snapshot is truncated");

    const auto body_size = bytes.size() - sizeof(std::uint32_t);
    reader crc_reader{std::span<const std::byte>{bytes}.subspan(body_size)};
    const auto stored_crc = crc_reader.scalar<std::uint32_t>();
    if (checksums_ && stored_crc != crc32c(std::span<const std::byte>{bytes}.first(body_size))) {
        throw corruption_error("snapshot checksum mismatch");
    }

    reader r{std::span<const std::byte>{bytes}.first(body_size)};
    if (r.scalar<std::uint64_t>() != snapshot_magic) throw corruption_error("snapshot magic mismatch");
    if (r.scalar<std::uint32_t>() != snapshot_format) throw corruption_error("unsupported snapshot format");

    snapshot_image image;
    image.sequence = r.scalar<sequence_number>();
    const auto roots = r.scalar<std::uint64_t>();
    if (roots > 10'000'000ULL) throw corruption_error("snapshot root count is implausible");
    for (std::uint64_t i = 0; i < roots; ++i) {
        auto name = r.string();
        root_record record;
        record.version = r.scalar<version_type>();
        record.type = r.string();
        const auto size = r.scalar<std::uint64_t>();
        if (size > r.remaining()) throw corruption_error("snapshot root payload exceeds file");
        const auto payload = r.bytes(static_cast<std::size_t>(size));
        record.payload.assign(payload.begin(), payload.end());
        image.roots.emplace(std::move(name), std::move(record));
    }
    if (!r.eof()) throw corruption_error("snapshot contains trailing bytes");
    return image;
}

void snapshot_store::save(const snapshot_image& image) const {
    writer w;
    w.scalar(snapshot_magic);
    w.scalar(snapshot_format);
    w.scalar(image.sequence);
    w.scalar<std::uint64_t>(image.roots.size());
    for (const auto& [name, record] : image.roots) {
        w.string(name);
        w.scalar(record.version);
        w.string(record.type);
        w.scalar<std::uint64_t>(record.payload.size());
        w.bytes(record.payload);
    }
    w.scalar(checksums_ ? crc32c(w.data()) : 0U);

    const auto temporary = path_.string() + ".tmp";
    auto file = storage_->open_file(temporary, true);
    file->truncate(0);
    file->write_exact(0, w.data());
    if (durability_ == durability_mode::strict) file->flush_all();
    file.reset();

    storage_->atomic_replace(temporary, path_);
    if (durability_ == durability_mode::strict) storage_->sync_directory(path_.parent_path());
}

} // namespace opheap::detail
