#pragma once

#include "opheap/detail/binary.hpp"

#include <concepts>
#include <memory_resource>
#include <string_view>

namespace opheap {

template<class T, class Enable = void>
struct codec;

template<class T>
concept persistable = requires(const T& source, detail::writer& w, detail::reader& r,
                               std::pmr::memory_resource* resource) {
    { codec<T>::type_name } -> std::convertible_to<std::string_view>;
    codec<T>::encode(w, source);
    { codec<T>::decode(r, resource) } -> std::same_as<T>;
};

} // namespace opheap
