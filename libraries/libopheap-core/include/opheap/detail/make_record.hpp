#pragma once

#include "opheap/detail/crc32c.hpp"
#include "opheap/detail/journal_format.hpp"
#include "opheap/detail/record_type.hpp"
#include "opheap/detail/writer.hpp"
#include "opheap/ids.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace opheap::detail {

inline std::vector<std::byte> make_record(record_type type, sequence_number sequence,
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

} // namespace opheap::detail
