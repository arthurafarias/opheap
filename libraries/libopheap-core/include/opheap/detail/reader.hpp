#pragma once

#include "opheap/corruption_error.hpp"
#include "opheap/detail/scalar_base.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <type_traits>

namespace opheap::detail {

struct reader {
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

} // namespace opheap::detail
