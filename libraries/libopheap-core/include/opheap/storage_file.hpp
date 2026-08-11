#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace opheap {

struct storage_file {
public:
    virtual ~storage_file() = default;
    [[nodiscard]] virtual std::uint64_t size() const = 0;
    virtual void read_exact(std::uint64_t offset, std::span<std::byte> output) const = 0;
    virtual void write_exact(std::uint64_t offset, std::span<const std::byte> data) = 0;
    [[nodiscard]] virtual std::uint64_t append(std::span<const std::byte> data) = 0;
    virtual void truncate(std::uint64_t new_size) = 0;
    virtual void flush_data() = 0;
    virtual void flush_all() = 0;
};

} // namespace opheap
