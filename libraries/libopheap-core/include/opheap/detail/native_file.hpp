#pragma once

#include "opheap/corruption_error.hpp"
#include "opheap/detail/io_error.hpp"
#include "opheap/storage_error.hpp"
#include "opheap/storage_file.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace opheap::detail {

#if defined(_WIN32)
struct native_file final : public storage_file {
public:
  native_file(std::filesystem::path path, bool create) : path_(std::move(path)) {
    handle_ = CreateFileW(path_.wstring().c_str(), GENERIC_READ | GENERIC_WRITE,
                          FILE_SHARE_READ, nullptr,
                          create ? OPEN_ALWAYS : OPEN_EXISTING,
                          FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle_ == INVALID_HANDLE_VALUE) io_error("CreateFile", path_);
  }
  ~native_file() override { if (handle_ != INVALID_HANDLE_VALUE) CloseHandle(handle_); }

  std::uint64_t size() const override {
    LARGE_INTEGER s{};
    if (!GetFileSizeEx(handle_, &s)) io_error("GetFileSizeEx", path_);
    return static_cast<std::uint64_t>(s.QuadPart);
  }

  void read_exact(std::uint64_t offset, std::span<std::byte> out) const override {
    std::size_t done = 0;
    while (done < out.size()) {
      OVERLAPPED ov{};
      const auto at = offset + done;
      ov.Offset = static_cast<DWORD>(at & 0xffffffffULL);
      ov.OffsetHigh = static_cast<DWORD>(at >> 32U);
      DWORD got = 0;
      const DWORD ask = static_cast<DWORD>(std::min<std::size_t>(out.size() - done, 1U << 30U));
      if (!ReadFile(handle_, out.data() + done, ask, &got, &ov) || got == 0) {
        io_error("ReadFile", path_);
      }
      done += got;
    }
  }

  void write_exact(std::uint64_t offset, std::span<const std::byte> data) override {
    std::size_t done = 0;
    while (done < data.size()) {
      OVERLAPPED ov{};
      const auto at = offset + done;
      ov.Offset = static_cast<DWORD>(at & 0xffffffffULL);
      ov.OffsetHigh = static_cast<DWORD>(at >> 32U);
      DWORD wrote = 0;
      const DWORD ask = static_cast<DWORD>(std::min<std::size_t>(data.size() - done, 1U << 30U));
      if (!WriteFile(handle_, data.data() + done, ask, &wrote, &ov) || wrote == 0) {
        io_error("WriteFile", path_);
      }
      done += wrote;
    }
  }

  std::uint64_t append(std::span<const std::byte> data) override {
    const auto off = size();
    write_exact(off, data);
    return off;
  }

  void truncate(std::uint64_t new_size) override {
    LARGE_INTEGER pos{};
    pos.QuadPart = static_cast<LONGLONG>(new_size);
    if (!SetFilePointerEx(handle_, pos, nullptr, FILE_BEGIN) || !SetEndOfFile(handle_)) {
      io_error("SetEndOfFile", path_);
    }
  }

  void flush_data() override { if (!FlushFileBuffers(handle_)) io_error("FlushFileBuffers", path_); }
  void flush_all() override { flush_data(); }

private:
  std::filesystem::path path_;
  HANDLE handle_{INVALID_HANDLE_VALUE};
};
#else
struct native_file final : public storage_file {
public:
  native_file(std::filesystem::path path, bool create) : path_(std::move(path)) {
    int flags = O_RDWR | O_CLOEXEC;
    if (create) flags |= O_CREAT;
    fd_ = ::open(path_.c_str(), flags, 0644);
    if (fd_ < 0) io_error("open", path_);
  }

  ~native_file() override { if (fd_ >= 0) ::close(fd_); }

  std::uint64_t size() const override {
    struct stat st{};
    if (::fstat(fd_, &st) != 0) io_error("fstat", path_);
    return static_cast<std::uint64_t>(st.st_size);
  }

  void read_exact(std::uint64_t offset, std::span<std::byte> out) const override {
    std::size_t done = 0;
    while (done < out.size()) {
      const auto rc = ::pread(fd_, out.data() + done, out.size() - done,
                              static_cast<off_t>(offset + done));
      if (rc == 0) throw corruption_error("unexpected EOF while reading " + path_.string());
      if (rc < 0) {
        if (errno == EINTR) continue;
        io_error("pread", path_);
      }
      done += static_cast<std::size_t>(rc);
    }
  }

  void write_exact(std::uint64_t offset, std::span<const std::byte> data) override {
    std::size_t done = 0;
    while (done < data.size()) {
      const auto rc = ::pwrite(fd_, data.data() + done, data.size() - done,
                               static_cast<off_t>(offset + done));
      if (rc < 0) {
        if (errno == EINTR) continue;
        io_error("pwrite", path_);
      }
      if (rc == 0) throw storage_error("zero-byte pwrite to " + path_.string());
      done += static_cast<std::size_t>(rc);
    }
  }

  std::uint64_t append(std::span<const std::byte> data) override {
    const auto off = size();
    write_exact(off, data);
    return off;
  }

  void truncate(std::uint64_t new_size) override {
    if (::ftruncate(fd_, static_cast<off_t>(new_size)) != 0) io_error("ftruncate", path_);
  }

  void flush_data() override {
#if defined(__APPLE__)
    if (::fsync(fd_) != 0) io_error("fsync", path_);
#else
    if (::fdatasync(fd_) != 0) io_error("fdatasync", path_);
#endif
  }

  void flush_all() override { if (::fsync(fd_) != 0) io_error("fsync", path_); }

private:
  std::filesystem::path path_;
  int fd_{-1};
};
#endif

} // namespace opheap::detail
