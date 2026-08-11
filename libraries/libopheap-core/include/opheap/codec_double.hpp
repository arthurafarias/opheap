#pragma once

#include "opheap/detail/binary.hpp"
#include "opheap/detail/persistable.hpp"

#include <memory_resource>
#include <string_view>

namespace opheap {

template<>
struct codec<double> {
    static constexpr std::string_view type_name = "double";
    static void encode(detail::writer& w, double value) { w.floating(value); }
    static double decode(detail::reader& r, std::pmr::memory_resource*) { return r.f64(); }
};

} // namespace opheap
