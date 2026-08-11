#pragma once

#include "opheap/corruption_error.hpp"
#include "opheap/detail/bind_if_supported.hpp"
#include "opheap/detail/binary.hpp"
#include "opheap/detail/persistable.hpp"
#include "opheap/map.hpp"

#include <cstdint>
#include <memory_resource>
#include <string_view>
#include <utility>

namespace opheap {

template<class K, detail::bindable V, class Compare>
requires persistable<K> && persistable<V>
struct codec<map<K, V, Compare>> {
    static constexpr std::string_view type_name = "opheap.map.v1";
    static void encode(detail::writer& w, const map<K, V, Compare>& values) {
        w.scalar<std::uint64_t>(values.size());
        for (const auto& [key, value] : values) {
            codec<K>::encode(w, key);
            codec<V>::encode(w, value);
        }
    }
    static map<K, V, Compare> decode(detail::reader& r, std::pmr::memory_resource* resource) {
        map<K, V, Compare> result{resource};
        const auto count = r.scalar<std::uint64_t>();
        if (count > r.remaining() + 1U) throw corruption_error("implausible map element count");
        for (std::uint64_t i = 0; i < count; ++i) {
            auto key = codec<K>::decode(r, resource);
            auto mapped = codec<V>::decode(r, resource);
            auto [it, inserted] = result.try_emplace(key);
            (void)inserted;
            it->second = std::move(mapped);
        }
        return result;
    }
};

} // namespace opheap
