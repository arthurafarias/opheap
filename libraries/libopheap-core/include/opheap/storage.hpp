#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>

namespace opheap {

class storage_file {
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

class storage_backend {
public:
    virtual ~storage_backend() = default;
    virtual std::unique_ptr<storage_file> open_file(const std::filesystem::path& path,
                                                    bool create_if_missing) = 0;
    virtual void create_directories(const std::filesystem::path& path) = 0;
    virtual bool exists(const std::filesystem::path& path) const = 0;
    virtual void atomic_replace(const std::filesystem::path& from,
                                const std::filesystem::path& to) = 0;
    virtual void remove_file(const std::filesystem::path& path) = 0;
    virtual void sync_directory(const std::filesystem::path& directory) = 0;
};

std::shared_ptr<storage_backend> make_default_storage_backend();

} // namespace opheap
