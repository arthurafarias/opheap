#pragma once

#include "opheap/storage_backend.hpp"

#include <memory>

namespace opheap {

std::shared_ptr<storage_backend> make_default_storage_backend();

} // namespace opheap
