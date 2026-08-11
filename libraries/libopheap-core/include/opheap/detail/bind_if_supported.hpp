#pragma once

#include "opheap/binding.hpp"

namespace opheap::detail {

template<class T>
concept bindable = requires(T& value, binding b) {
    value.bind(b);
};

template<class T>
void bind_if_supported(T& value, binding b) {
    if constexpr (bindable<T>) {
        value.bind(b);
    } else {
        (void)value;
        (void)b;
    }
}

} // namespace opheap::detail
