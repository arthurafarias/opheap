#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace opheap::detail {

inline std::uint32_t crc32c(std::span<const std::byte> data) noexcept {
    std::uint32_t crc = 0xffffffffU;
    for (auto byte : data) {
        crc ^= std::to_integer<std::uint8_t>(byte);
        for (int bit = 0; bit < 8; ++bit) {
            const auto mask = static_cast<std::uint32_t>(-(static_cast<std::int32_t>(crc & 1U)));
            crc = (crc >> 1U) ^ (0x82f63b78U & mask);
        }
    }
    return ~crc;
}

} // namespace opheap::detail
