#pragma once

#include "opheap/detail/binary.hpp"
#include "opheap/detail/persistable.hpp"

#include <memory_resource>
#include <string>
#include <string_view>

namespace opheap {

template<>
struct codec<std::string> {
    static constexpr std::string_view type_name = "std.string.v1";
    static void encode(detail::writer& w, const std::string& value) { w.string(value); }
    static std::string decode(detail::reader& r, std::pmr::memory_resource*) { return r.string(); }
};

} // namespace opheap
