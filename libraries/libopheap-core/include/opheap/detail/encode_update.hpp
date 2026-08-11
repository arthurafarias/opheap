#pragma once

#include "opheap/detail/encoded_update.hpp"
#include "opheap/detail/root_update.hpp"
#include "opheap/detail/writer.hpp"

#include <cstdint>

namespace opheap::detail {

inline encoded_update encode_update(const root_update& update) {
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

} // namespace opheap::detail
