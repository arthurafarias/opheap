#pragma once

#include "opheap/detail/binary.hpp"
#include "opheap/detail/persistable.hpp"
#include "opheap/string.hpp"

#include <memory_resource>
#include <string_view>

namespace opheap {

template<>
struct codec<string> {
    static constexpr std::string_view type_name = "opheap.string.v1";
    static void encode(detail::writer& w, const string& value) { w.string(value.view()); }
    static string decode(detail::reader& r, std::pmr::memory_resource* resource) {
        return string{r.string(), resource};
    }
};

} // namespace opheap
