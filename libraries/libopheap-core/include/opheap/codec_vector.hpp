#pragma once

#include "opheap/corruption_error.hpp"
#include "opheap/detail/bind_if_supported.hpp"
#include "opheap/detail/binary.hpp"
#include "opheap/detail/persistable.hpp"
#include "opheap/vector.hpp"

#include <cstdint>
#include <memory_resource>
#include <string_view>

namespace opheap {

template<detail::bindable T>
requires persistable<T>
struct codec<vector<T>> {
    static constexpr std::string_view type_name = "opheap.vector.v1";
    static void encode(detail::writer& w, const vector<T>& values) {
        w.scalar<std::uint64_t>(values.size());
        for (const auto& value : values) codec<T>::encode(w, value);
    }
    static vector<T> decode(detail::reader& r, std::pmr::memory_resource* resource) {
        vector<T> result{resource};
        const auto count = r.scalar<std::uint64_t>();
        if (count > r.remaining() + 1U) throw corruption_error("implausible vector element count");
        result.reserve(static_cast<std::size_t>(count));
        for (std::uint64_t i = 0; i < count; ++i) {
            result.push_back(codec<T>::decode(r, resource));
        }
        return result;
    }
};

} // namespace opheap
