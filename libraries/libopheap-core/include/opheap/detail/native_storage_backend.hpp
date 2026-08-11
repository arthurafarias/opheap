#pragma once

#include "opheap/detail/io_error.hpp"
#include "opheap/detail/native_file.hpp"
#include "opheap/storage_backend.hpp"
#include "opheap/storage_error.hpp"

#include <filesystem>
#include <memory>
#include <system_error>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace opheap::detail {

struct native_storage_backend final : public storage_backend {
public:
  std::unique_ptr<storage_file> open_file(const std::filesystem::path& path,
                                         bool create_if_missing) override {
    return std::make_unique<native_file>(path, create_if_missing);
  }

  void create_directories(const std::filesystem::path& path) override {
    std::error_code ec;
    std::filesystem::create_directories(path, ec);
    if (ec) throw storage_error("create_directories failed: " + ec.message());
  }

  bool exists(const std::filesystem::path& path) const override {
    std::error_code ec;
    const bool value = std::filesystem::exists(path, ec);
    if (ec) throw storage_error("exists failed: " + ec.message());
    return value;
  }

  void atomic_replace(const std::filesystem::path& from,
                      const std::filesystem::path& to) override {
#if defined(_WIN32)
    if (!MoveFileExW(from.wstring().c_str(), to.wstring().c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
      io_error("MoveFileEx", to);
    }
#else
    if (::rename(from.c_str(), to.c_str()) != 0) io_error("rename", to);
#endif
  }

  void remove_file(const std::filesystem::path& path) override {
    std::error_code ec;
    std::filesystem::remove(path, ec);
    if (ec) throw storage_error("remove failed: " + ec.message());
  }

  void sync_directory(const std::filesystem::path& directory) override {
#if defined(_WIN32)
    // MoveFileEx(..., MOVEFILE_WRITE_THROUGH) is the strongest generally
    // available directory-entry persistence primitive used by this backend.
    (void)directory;
#else
    const int fd = ::open(directory.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (fd < 0) io_error("open directory", directory);
    const int rc = ::fsync(fd);
    const int saved = errno;
    ::close(fd);
    if (rc != 0) {
      errno = saved;
      io_error("fsync directory", directory);
    }
#endif
  }
};

} // namespace opheap::detail
