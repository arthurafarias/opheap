#include "opheap/detail/journal.hpp"

#include "opheap/detail/binary.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <span>
#include <unordered_map>

namespace opheap::detail {
namespace {

constexpr std::uint32_t record_magic = 0x4f504a52U; // OPJR
constexpr std::uint16_t record_format = 1;
constexpr std::size_t prefix_size = sizeof(std::uint32_t) + sizeof(std::uint64_t);
constexpr std::size_t body_prefix_size = sizeof(std::uint16_t) + sizeof(std::uint16_t) +
                                         sizeof(sequence_number) + sizeof(transaction_id) +
                                         sizeof(std::uint64_t);

enum class record_type : std::uint16_t { begin = 1, root_update = 2, commit = 3 };

struct encoded_update {
    std::vector<std::byte> bytes;
    std::size_t payload_offset{};
};

struct recovered_update {
    std::string name;
    version_type expected_version{};
    version_type new_version{};
    std::string type;
    loc payload;
};

std::vector<std::byte> make_record(record_type type, sequence_number sequence,
                                   transaction_id transaction,
                                   std::span<const std::byte> payload,
                                   bool checksums) {
    writer body;
    body.scalar(record_format);
    body.scalar(type);
    body.scalar(sequence);
    body.scalar(transaction);
    body.scalar<std::uint64_t>(payload.size());
    body.bytes(payload);
    const auto checksum = checksums ? crc32c(body.data()) : 0U;

    writer frame;
    frame.scalar(record_magic);
    frame.scalar<std::uint64_t>(body.data().size() + sizeof(std::uint32_t));
    frame.bytes(body.data());
    frame.scalar(checksum);
    return std::move(frame).take();
}

encoded_update encode_update(const root_update& update) {
    writer w;
    w.string(update.name);
    w.scalar(update.expected_version);
    w.scalar(update.new_version);
    w.string(update.type);
    w.scalar<std::uint64_t>(update.payload.size());
    const auto payload_offset = w.data().size();
    w.bytes(update.payload);
    return {std::move(w).take(), payload_offset};
}

recovered_update decode_update(std::span<const std::byte> bytes,
                                std::uint64_t file_payload_offset) {
    reader r{bytes};
    recovered_update result;
    result.name = r.string();
    result.expected_version = r.scalar<version_type>();
    result.new_version = r.scalar<version_type>();
    result.type = r.string();
    const auto count = r.scalar<std::uint64_t>();
    if (count > r.remaining()) throw corruption_error("journal root payload length exceeds record");
    const auto payload_offset = bytes.size() - r.remaining();
    const auto payload = r.bytes(static_cast<std::size_t>(count));
    if (!r.eof()) throw corruption_error("journal root update has trailing bytes");
    result.payload = loc{source::journal,
                         file_payload_offset + payload_offset,
                         count,
                         crc32c(payload)};
    return result;
}

} // namespace

journal::journal(std::filesystem::path path,
                 std::shared_ptr<storage_backend> storage,
                 durability_mode durability,
                 bool checksums)
    : path_(std::move(path)), storage_(std::move(storage)),
      file_(storage_->open_file(path_, true)), durability_(durability), checksums_(checksums) {}

recovery_result journal::recover(std::unordered_map<std::string, root_record> base,
                                 sequence_number snapshot_sequence,
                                 bool truncate_torn_tail) {
    struct pending_transaction {
        std::vector<recovered_update> updates;
        bool began{};
    };

    recovery_result result;
    result.roots = std::move(base);
    result.last_sequence = snapshot_sequence;

    std::unordered_map<transaction_id, pending_transaction> pending;
    std::uint64_t offset = 0;
    std::uint64_t last_good = 0;
    const auto file_size = file_->size();

    while (offset < file_size) {
        if (file_size - offset < prefix_size) break;
        std::array<std::byte, prefix_size> prefix{};
        file_->read_exact(offset, prefix);
        reader prefix_reader{prefix};
        if (prefix_reader.scalar<std::uint32_t>() != record_magic) {
            throw corruption_error("journal magic mismatch");
        }
        const auto frame_size = prefix_reader.scalar<std::uint64_t>();
        if (frame_size < sizeof(std::uint32_t) || frame_size > (1ULL << 40U)) {
            throw corruption_error("journal record length is invalid");
        }
        if (frame_size > file_size - offset - prefix_size) break;

        std::vector<std::byte> frame(static_cast<std::size_t>(frame_size));
        file_->read_exact(offset + prefix_size, frame);
        if (frame.size() < sizeof(std::uint32_t)) throw corruption_error("journal record too small");

        const auto body_size = frame.size() - sizeof(std::uint32_t);
        reader checksum_reader{std::span<const std::byte>{frame}.subspan(body_size)};
        const auto stored_checksum = checksum_reader.scalar<std::uint32_t>();
        if (checksums_) {
            const auto calculated = crc32c(std::span<const std::byte>{frame}.first(body_size));
            if (stored_checksum != calculated) throw corruption_error("journal checksum mismatch");
        }

        reader r{std::span<const std::byte>{frame}.first(body_size)};
        if (r.scalar<std::uint16_t>() != record_format) throw corruption_error("unsupported journal format");
        const auto type = r.scalar<record_type>();
        const auto sequence = r.scalar<sequence_number>();
        const auto transaction = r.scalar<transaction_id>();
        const auto payload_size = r.scalar<std::uint64_t>();
        if (payload_size > r.remaining()) throw corruption_error("journal payload length mismatch");
        const auto payload = r.bytes(static_cast<std::size_t>(payload_size));
        if (!r.eof()) throw corruption_error("journal record has trailing bytes");

        if (sequence <= result.last_sequence && sequence > snapshot_sequence) {
            throw corruption_error("journal sequence is not strictly increasing");
        }
        result.last_sequence = std::max(result.last_sequence, sequence);

        switch (type) {
        case record_type::begin:
            pending[transaction].began = true;
            break;
        case record_type::root_update: {
            if (!pending[transaction].began) throw corruption_error("root update without BEGIN");
            const auto payload_file_offset = offset + prefix_size + body_prefix_size;
            pending[transaction].updates.push_back(decode_update(payload, payload_file_offset));
            break;
        }
        case record_type::commit: {
            auto found = pending.find(transaction);
            if (found == pending.end() || !found->second.began) throw corruption_error("COMMIT without BEGIN");
            reader count_reader{payload};
            const auto expected_count = count_reader.scalar<std::uint64_t>();
            if (!count_reader.eof() || expected_count != found->second.updates.size()) {
                throw corruption_error("COMMIT update count mismatch");
            }
            for (auto& update : found->second.updates) {
                auto existing = result.roots.find(update.name);
                const auto current_version = existing == result.roots.end() ? version_type{} : existing->second.version;
                if (update.new_version <= current_version) continue;
                if (current_version != update.expected_version) {
                    throw corruption_error("journal root version chain mismatch for " + update.name);
                }
                result.roots[update.name] = root_record{
                    update.new_version, std::move(update.type), update.payload};
            }
            pending.erase(found);
            break;
        }
        default:
            throw corruption_error("unknown journal record type");
        }

        ++result.records;
        offset += prefix_size + frame_size;
        last_good = offset;
    }

    result.valid_bytes = last_good;
    if (truncate_torn_tail && last_good != file_size) {
        file_->truncate(last_good);
        if (durability_ == durability_mode::strict) file_->flush_all();
    }
    last_sequence_ = result.last_sequence;
    return result;
}

std::vector<loc> journal::append(transaction_id tx, const std::vector<root_update>& updates) {
    std::vector<std::byte> batch;
    std::vector<loc> locations;
    locations.reserve(updates.size());

    auto append_record = [&](record_type type, std::span<const std::byte> payload) {
        auto bytes = make_record(type, ++last_sequence_, tx, payload, checksums_);
        batch.insert(batch.end(), bytes.begin(), bytes.end());
    };

    append_record(record_type::begin, {});
    for (const auto& update : updates) {
        auto encoded = encode_update(update);
        const auto record_start = batch.size();
        append_record(record_type::root_update, encoded.bytes);
        locations.push_back(loc{
            source::journal,
            static_cast<std::uint64_t>(record_start + prefix_size + body_prefix_size + encoded.payload_offset),
            static_cast<std::uint64_t>(update.payload.size()),
            crc32c(update.payload)});
    }
    writer commit_payload;
    commit_payload.scalar<std::uint64_t>(updates.size());
    append_record(record_type::commit, commit_payload.data());

    const auto append_offset = file_->append(batch);
    for (auto& where : locations) where.offset += append_offset;
    if (durability_ == durability_mode::strict) file_->flush_data();
    return locations;
}

void journal::read(const loc& where, std::uint64_t offset, std::span<std::byte> out) const {
    if (where.kind != source::journal) throw storage_error("journal read with non-journal locator");
    if (offset > where.size || out.size() > where.size - offset) {
        throw corruption_error("journal payload read exceeds locator");
    }
    file_->read_exact(where.offset + offset, out);
}

std::vector<std::byte> journal::load(const loc& where) const {
    if (where.size > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        throw storage_error("journal payload is too large for this process");
    }
    std::vector<std::byte> bytes(static_cast<std::size_t>(where.size));
    read(where, 0, bytes);
    if (checksums_ && crc32c(bytes) != where.checksum) {
        throw corruption_error("journal payload checksum mismatch");
    }
    return bytes;
}

void journal::reset() {
    file_->truncate(0);
    if (durability_ == durability_mode::strict) file_->flush_all();
}

} // namespace opheap::detail
