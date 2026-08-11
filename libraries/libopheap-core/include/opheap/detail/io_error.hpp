#pragma once

#include "opheap/storage_error.hpp"

#include <cerrno>
#include <cstring>
#include <filesystem>
#include <string>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#endif

namespace opheap::detail {

[[noreturn]] inline void io_error(const char* operation, const std::filesystem::path& path) {
#if defined(_WIN32)
  throw storage_error(std::string(operation) + " failed for " + path.string() +
                         " (win32=" + std::to_string(GetLastError()) + ")");
#else
  throw storage_error(std::string(operation) + " failed for " + path.string() +
                         ": " + std::strerror(errno));
#endif
}

} // namespace opheap::detail
