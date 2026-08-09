#include "opheap/detail/snapshot.hpp"

#include "opheap/detail/binary.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <string>
#include <vector>

namespace opheap::detail {
namespace {
constexpr std::uint64_t snapshot_magic = 0x3250414e53504fULL; // "OPSNAP2"
constexpr std::uint32_t snapshot_format = 2;
constexpr std::size_t header_size = sizeof(std::uint64_t) + sizeof(std::uint32_t) +
                                    sizeof(sequence_number) + sizeof(std::uint64_t) +
                                    sizeof(std::uint64_t) + sizeof(std::uint32_t) +
                                    sizeof(std::uint32_t);
constexpr std::size_t copy_chunk = 64U * 1024U;

struct indexed_root {
    std::string name;
    root_record record;
    std::uint64_t relative_offset{};
};

std::vector<indexed_root> ordered_roots(const snapshot_image& image) {
    std::vector<indexed_root> roots;
    roots.reserve(image.roots.size());
    for (const auto& [name, record] : image.roots) roots.push_back({name, record, 0});
    std::sort(roots.begin(), roots.end(), [](const auto& a, const auto& b) {
        return a.name < b.name;
    });
    std::uint64_t offset = 0;
    for (auto& root : roots) {
        root.relative_offset = offset;
        if (root.record.payload.size > std::numeric_limits<std::uint64_t>::max() - offset) {
            throw storage_error("snapshot payload size overflow");
        }
        offset += root.record.payload.size;
    }
    return roots;
}

} // namespace

snapshot_image snapshot_store::load() const {
    if (!storage_->exists(path_)) return {};
    auto file = storage_->open_file(path_, false);
    if (file->size() == 0) return {};
    if (file->size() < header_size) throw corruption_error("snapshot is truncated");

    std::array<std::byte, header_size> header{};
    file->read_exact(0, header);
    reader h{header};
    if (h.scalar<std::uint64_t>() != snapshot_magic) throw corruption_error("snapshot magic mismatch");
    if (h.scalar<std::uint32_t>() != snapshot_format) throw corruption_error("unsupported snapshot format");

    snapshot_image image;
    image.sequence = h.scalar<sequence_number>();
    const auto root_count = h.scalar<std::uint64_t>();
    const auto index_size = h.scalar<std::uint64_t>();
    const auto index_crc = h.scalar<std::uint32_t>();
    const auto header_crc = h.scalar<std::uint32_t>();
    if (!h.eof()) throw corruption_error("snapshot header contains trailing bytes");
    if (checksums_ && header_crc != crc32c(std::span<const std::byte>{header}.first(header_size - sizeof(std::uint32_t)))) {
        throw corruption_error("snapshot header checksum mismatch");
    }
    if (root_count > 10'000'000ULL) throw corruption_error("snapshot root count is implausible");
    if (index_size > file->size() - header_size) throw corruption_error("snapshot index exceeds file");
    if (index_size > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        throw storage_error("snapshot index is too large for this process");
    }

    std::vector<std::byte> index(static_cast<std::size_t>(index_size));
    if (!index.empty()) file->read_exact(header_size, index);
    if (checksums_ && index_crc != crc32c(index)) throw corruption_error("snapshot index checksum mismatch");

    reader r{index};
    const auto data_base = static_cast<std::uint64_t>(header_size) + index_size;
    for (std::uint64_t i = 0; i < root_count; ++i) {
        auto name = r.string();
        root_record record;
        record.version = r.scalar<version_type>();
        record.type = r.string();
        const auto relative = r.scalar<std::uint64_t>();
        const auto size = r.scalar<std::uint64_t>();
        const auto checksum = r.scalar<std::uint32_t>();
        if (relative > file->size() - data_base || size > file->size() - data_base - relative) {
            throw corruption_error("snapshot payload locator exceeds file");
        }
        record.payload = loc{source::snapshot, data_base + relative, size, checksum};
        if (!image.roots.emplace(std::move(name), std::move(record)).second) {
            throw corruption_error("snapshot contains duplicate root");
        }
    }
    if (!r.eof()) throw corruption_error("snapshot index contains trailing bytes");
    return image;
}

snapshot_image snapshot_store::save(const snapshot_image& image,
                                    const payload_reader& read_payload) const {
    auto roots = ordered_roots(image);

    writer index;
    for (const auto& root : roots) {
        index.string(root.name);
        index.scalar(root.record.version);
        index.string(root.record.type);
        index.scalar(root.relative_offset);
        index.scalar(root.record.payload.size);
        index.scalar(root.record.payload.checksum);
    }

    writer header_without_crc;
    header_without_crc.scalar(snapshot_magic);
    header_without_crc.scalar(snapshot_format);
    header_without_crc.scalar(image.sequence);
    header_without_crc.scalar<std::uint64_t>(roots.size());
    header_without_crc.scalar<std::uint64_t>(index.data().size());
    header_without_crc.scalar(checksums_ ? crc32c(index.data()) : 0U);

    writer header;
    header.bytes(header_without_crc.data());
    header.scalar(checksums_ ? crc32c(header_without_crc.data()) : 0U);
    if (header.data().size() != header_size) throw storage_error("internal snapshot header size mismatch");

    const auto temporary = path_.string() + ".tmp";
    auto file = storage_->open_file(temporary, true);
    file->truncate(0);
    file->write_exact(0, header.data());
    if (!index.data().empty()) file->write_exact(header_size, index.data());

    const auto data_base = static_cast<std::uint64_t>(header_size + index.data().size());
    std::vector<std::byte> chunk(copy_chunk);
    for (const auto& root : roots) {
        std::uint64_t copied = 0;
        while (copied < root.record.payload.size) {
            const auto amount = static_cast<std::size_t>(std::min<std::uint64_t>(chunk.size(), root.record.payload.size - copied));
            auto out = std::span<std::byte>{chunk}.first(amount);
            read_payload(root.record, copied, out);
            file->write_exact(data_base + root.relative_offset + copied, out);
            copied += amount;
        }
    }

    if (durability_ == durability_mode::strict) file->flush_all();
    file.reset();
    storage_->atomic_replace(temporary, path_);
    if (durability_ == durability_mode::strict) storage_->sync_directory(path_.parent_path());

    snapshot_image result;
    result.sequence = image.sequence;
    for (const auto& root : roots) {
        result.roots.emplace(root.name, root_record{
            root.record.version,
            root.record.type,
            loc{source::snapshot,
                data_base + root.relative_offset,
                root.record.payload.size,
                root.record.payload.checksum}});
    }
    return result;
}

void snapshot_store::read(const loc& where, std::uint64_t offset, std::span<std::byte> out) const {
    if (where.kind != source::snapshot) throw storage_error("snapshot read with non-snapshot locator");
    if (offset > where.size || out.size() > where.size - offset) {
        throw corruption_error("snapshot payload read exceeds locator");
    }
    auto file = storage_->open_file(path_, false);
    file->read_exact(where.offset + offset, out);
}

std::vector<std::byte> snapshot_store::load(const loc& where) const {
    if (where.size > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        throw storage_error("snapshot payload is too large for this process");
    }
    std::vector<std::byte> bytes(static_cast<std::size_t>(where.size));
    read(where, 0, bytes);
    if (checksums_ && crc32c(bytes) != where.checksum) {
        throw corruption_error("snapshot payload checksum mismatch");
    }
    return bytes;
}

} // namespace opheap::detail
