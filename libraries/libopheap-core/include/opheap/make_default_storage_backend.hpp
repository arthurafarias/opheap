#pragma once

#include "opheap/detail/native_storage_backend.hpp"
#include "opheap/storage_backend.hpp"

#include <memory>

namespace opheap {

inline std::shared_ptr<storage_backend> make_default_storage_backend() {
    return std::make_shared<detail::native_storage_backend>();
}

} // namespace opheap
