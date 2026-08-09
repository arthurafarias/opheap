#pragma once

#include "opheap/types.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace opheap::detail {

template<class T, bool = std::is_enum_v<T>>
struct scalar_base { using type = T; };
template<class T>
struct scalar_base<T, true> { using type = std::underlying_type_t<T>; };
template<class T>
using scalar_base_t = typename scalar_base<T>::type;

class writer {
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

class reader {
public:
    explicit reader(std::span<const std::byte> bytes) : bytes_(bytes) {}

    template<class T>
    requires (std::is_integral_v<T> || std::is_enum_v<T>)
    T scalar() {
        using raw = scalar_base_t<T>;
        using unsigned_type = std::make_unsigned_t<raw>;
        require(sizeof(unsigned_type));
        unsigned_type bits{};
        for (std::size_t i = 0; i < sizeof(unsigned_type); ++i) {
            bits |= static_cast<unsigned_type>(std::to_integer<unsigned char>(bytes_[position_ + i])) << (8U * i);
        }
        position_ += sizeof(unsigned_type);
        return static_cast<T>(static_cast<raw>(bits));
    }

    float f32() { return std::bit_cast<float>(scalar<std::uint32_t>()); }
    double f64() { return std::bit_cast<double>(scalar<std::uint64_t>()); }

    std::span<const std::byte> bytes(std::size_t count) {
        require(count);
        auto result = bytes_.subspan(position_, count);
        position_ += count;
        return result;
    }

    std::string string() {
        const auto count = scalar<std::uint64_t>();
        if (count > remaining()) throw corruption_error("string length exceeds payload");
        const auto data = bytes(static_cast<std::size_t>(count));
        return {reinterpret_cast<const char*>(data.data()), data.size()};
    }

    [[nodiscard]] std::size_t remaining() const noexcept { return bytes_.size() - position_; }
    [[nodiscard]] bool eof() const noexcept { return position_ == bytes_.size(); }

private:
    void require(std::size_t count) const {
        if (count > remaining()) throw corruption_error("truncated binary payload");
    }

    std::span<const std::byte> bytes_;
    std::size_t position_{};
};

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
