#pragma once

#include "opheap/ids.hpp"

#include <cstddef>
#include <cstdint>

namespace opheap::detail {

inline constexpr std::uint32_t record_magic = 0x4f504a52U; // OPJR
inline constexpr std::uint16_t record_format = 1;
inline constexpr std::size_t prefix_size = sizeof(std::uint32_t) + sizeof(std::uint64_t);
inline constexpr std::size_t body_prefix_size = sizeof(std::uint16_t) + sizeof(std::uint16_t) +
                                         sizeof(sequence_number) + sizeof(transaction_id) +
                                         sizeof(std::uint64_t);

} // namespace opheap::detail
