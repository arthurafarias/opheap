#pragma once

#include "opheap/detail/scalar_base.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <type_traits>
#include <vector>

namespace opheap::detail {

struct writer {
public:
    template<class T>
    requires (std::is_integral_v<T> || std::is_enum_v<T>)
    void scalar(T value) {
        using raw = scalar_base_t<T>;
        using unsigned_type = std::make_unsigned_t<raw>;
        auto bits = static_cast<unsigned_type>(static_cast<raw>(value));
        for (std::size_t i = 0; i < sizeof(unsigned_type); ++i) {
            bytes_.push_back(static_cast<std::byte>((bits >> (8U * i)) & unsigned_type{0xff}));
        }
    }

    void floating(float value) { scalar(std::bit_cast<std::uint32_t>(value)); }
    void floating(double value) { scalar(std::bit_cast<std::uint64_t>(value)); }

    void bytes(std::span<const std::byte> value) {
        bytes_.insert(bytes_.end(), value.begin(), value.end());
    }

    void string(std::string_view value) {
        scalar<std::uint64_t>(value.size());
        bytes(std::as_bytes(std::span{value.data(), value.size()}));
    }

    [[nodiscard]] const std::vector<std::byte>& data() const noexcept { return bytes_; }
    [[nodiscard]] std::vector<std::byte> take() && { return std::move(bytes_); }

private:
    std::vector<std::byte> bytes_;
};

} // namespace opheap::detail
