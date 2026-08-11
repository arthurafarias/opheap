#pragma once

#include "opheap/detail/binary.hpp"
#include "opheap/detail/persistable.hpp"
#include "opheap/property.hpp"

#include <memory_resource>
#include <string_view>

namespace opheap {

template<class T>
requires persistable<T>
struct codec<property<T>> {
    static constexpr std::string_view type_name = "opheap.property.v1";
    static void encode(detail::writer& w, const property<T>& value) {
        codec<T>::encode(w, value.get());
    }
    static property<T> decode(detail::reader& r, std::pmr::memory_resource* resource) {
        return property<T>{codec<T>::decode(r, resource)};
    }
};

} // namespace opheap
