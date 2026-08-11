#pragma once

namespace opheap {

struct null_t final {
    friend constexpr bool operator==(null_t, null_t) noexcept = default;
};
inline constexpr null_t null{};

} // namespace opheap
