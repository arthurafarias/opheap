#pragma once

#include <type_traits>

namespace opheap::detail {

template<class T, bool = std::is_enum_v<T>>
struct scalar_base { using type = T; };
template<class T>
struct scalar_base<T, true> { using type = std::underlying_type_t<T>; };
template<class T>
using scalar_base_t = typename scalar_base<T>::type;

} // namespace opheap::detail
