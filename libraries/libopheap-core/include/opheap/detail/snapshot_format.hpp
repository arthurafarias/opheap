#pragma once

#include "opheap/ids.hpp"

#include <cstddef>
#include <cstdint>

namespace opheap::detail {

inline constexpr std::uint64_t snapshot_magic = 0x3250414e53504fULL; // "OPSNAP2"
inline constexpr std::uint32_t snapshot_format = 2;
inline constexpr std::size_t header_size = sizeof(std::uint64_t) + sizeof(std::uint32_t) +
                                    sizeof(sequence_number) + sizeof(std::uint64_t) +
                                    sizeof(std::uint64_t) + sizeof(std::uint32_t) +
                                    sizeof(std::uint32_t);
inline constexpr std::size_t copy_chunk = 64U * 1024U;

} // namespace opheap::detail
