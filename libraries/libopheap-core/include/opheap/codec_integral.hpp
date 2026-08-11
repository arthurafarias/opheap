#pragma once

#include "opheap/detail/binary.hpp"
#include "opheap/detail/persistable.hpp"

#include <concepts>
#include <memory_resource>
#include <string_view>

namespace opheap {

template<std::integral T>
struct codec<T> {
    static constexpr std::string_view type_name = "integer";
    static void encode(detail::writer& w, T value) { w.scalar(value); }
    static T decode(detail::reader& r, std::pmr::memory_resource*) { return r.scalar<T>(); }
};

} // namespace opheap
