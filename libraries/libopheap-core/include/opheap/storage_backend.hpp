#pragma once

#include "opheap/storage_file.hpp"

#include <filesystem>
#include <memory>

namespace opheap {

struct storage_backend {
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

} // namespace opheap
