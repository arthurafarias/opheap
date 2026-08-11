#pragma once

#include "opheap/corruption_error.hpp"
#include "opheap/detail/crc32c.hpp"
#include "opheap/detail/loc.hpp"
#include "opheap/detail/reader.hpp"
#include "opheap/detail/recovered_update.hpp"
#include "opheap/detail/source.hpp"
#include "opheap/ids.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace opheap::detail {

inline recovered_update decode_update(std::span<const std::byte> bytes,
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

} // namespace opheap::detail
