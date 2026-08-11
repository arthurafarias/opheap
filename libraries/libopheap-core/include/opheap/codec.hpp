#pragma once

#include "opheap/detail/binary.hpp"
#include "opheap/property.hpp"
#include "opheap/value.hpp"

#include <concepts>
#include <cstdint>
#include <memory_resource>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>

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

template<std::integral T>
struct codec<T> {
    static constexpr std::string_view type_name = "integer";
    static void encode(detail::writer& w, T value) { w.scalar(value); }
    static T decode(detail::reader& r, std::pmr::memory_resource*) { return r.scalar<T>(); }
};

template<>
struct codec<double> {
    static constexpr std::string_view type_name = "double";
    static void encode(detail::writer& w, double value) { w.floating(value); }
    static double decode(detail::reader& r, std::pmr::memory_resource*) { return r.f64(); }
};

template<>
struct codec<std::string> {
    static constexpr std::string_view type_name = "std.string.v1";
    static void encode(detail::writer& w, const std::string& value) { w.string(value); }
    static std::string decode(detail::reader& r, std::pmr::memory_resource*) { return r.string(); }
};

template<>
struct codec<string> {
    static constexpr std::string_view type_name = "opheap.string.v1";
    static void encode(detail::writer& w, const string& value) { w.string(value.view()); }
    static string decode(detail::reader& r, std::pmr::memory_resource* resource) {
        return string{r.string(), resource};
    }
};

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

template<>
struct codec<array> {
    static constexpr std::string_view type_name = "opheap.array.v1";
    static void encode(detail::writer& w, const array& values);
    static array decode(detail::reader& r, std::pmr::memory_resource* resource);
};

template<>
struct codec<object> {
    static constexpr std::string_view type_name = "opheap.object.v1";
    static void encode(detail::writer& w, const object& values);
    static object decode(detail::reader& r, std::pmr::memory_resource* resource);
};

template<>
struct codec<value> {
    static constexpr std::string_view type_name = "opheap.value.v1";

    enum class tag : std::uint8_t { null, boolean, integer, number, string, array, object };

    static void encode(detail::writer& w, const value& item) {
        const auto& storage = item.storage();
        if (std::holds_alternative<null_t>(storage)) {
            w.scalar(tag::null);
        } else if (const auto* v = std::get_if<bool>(&storage)) {
            w.scalar(tag::boolean); w.scalar<std::uint8_t>(*v ? 1U : 0U);
        } else if (const auto* v = std::get_if<std::int64_t>(&storage)) {
            w.scalar(tag::integer); w.scalar(*v);
        } else if (const auto* v = std::get_if<double>(&storage)) {
            w.scalar(tag::number); w.floating(*v);
        } else if (const auto* v = std::get_if<string>(&storage)) {
            w.scalar(tag::string); codec<string>::encode(w, *v);
        } else if (const auto* v = std::get_if<value::array_ptr>(&storage)) {
            w.scalar(tag::array); codec<array>::encode(w, **v);
        } else {
            w.scalar(tag::object); codec<object>::encode(w, **std::get_if<value::object_ptr>(&storage));
        }
    }

    static value decode(detail::reader& r, std::pmr::memory_resource* resource) {
        switch (r.scalar<tag>()) {
        case tag::null: return value{null, resource};
        case tag::boolean: return value{r.scalar<std::uint8_t>() != 0, resource};
        case tag::integer: return value{r.scalar<std::int64_t>(), resource};
        case tag::number: return value{r.f64(), resource};
        case tag::string: {
            auto s = codec<string>::decode(r, resource);
            return value{s.view(), resource};
        }
        case tag::array: {
            value result{resource};
            auto decoded = codec<array>::decode(r, resource);
            auto& target = result.as_array();
            for (auto& element : decoded) target.push_back(std::move(element));
            return result;
        }
        case tag::object: {
            value result{resource};
            auto decoded = codec<object>::decode(r, resource);
            auto& target = result.as_object();
            for (auto& [key, mapped] : decoded) {
                auto [it, inserted] = target.try_emplace(key);
                (void)inserted;
                it->second = std::move(mapped);
            }
            return result;
        }
        }
        throw corruption_error("unknown value tag");
    }
};

inline void codec<array>::encode(detail::writer& w, const array& values) {
    w.scalar<std::uint64_t>(values.size());
    for (const auto& element : values) codec<opheap::value>::encode(w, element);
}
inline array codec<array>::decode(detail::reader& r, std::pmr::memory_resource* resource) {
    array result{resource};
    const auto count = r.scalar<std::uint64_t>();
    if (count > r.remaining() + 1U) throw corruption_error("implausible array element count");
    result.reserve(static_cast<std::size_t>(count));
    for (std::uint64_t i = 0; i < count; ++i) result.push_back(codec<value>::decode(r, resource));
    return result;
}
inline void codec<object>::encode(detail::writer& w, const object& values) {
    w.scalar<std::uint64_t>(values.size());
    for (const auto& [key, mapped] : values) {
        w.string(key);
        codec<value>::encode(w, mapped);
    }
}
inline object codec<object>::decode(detail::reader& r, std::pmr::memory_resource* resource) {
    object result{resource};
    const auto count = r.scalar<std::uint64_t>();
    if (count > r.remaining() + 1U) throw corruption_error("implausible object element count");
    for (std::uint64_t i = 0; i < count; ++i) {
        auto key = r.string();
        auto mapped = codec<value>::decode(r, resource);
        auto [it, inserted] = result.try_emplace(key);
        (void)inserted;
        it->second = std::move(mapped);
    }
    return result;
}

} // namespace opheap
