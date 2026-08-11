#pragma once

#include "opheap/observer.hpp"

#include <concepts>
#include <functional>
#include <type_traits>
#include <utility>

namespace opheap {

template<class T>
class property : public observable {
    static_assert(!std::is_pointer_v<T>, "opheap::property does not persist raw pointers");
    static_assert(!std::is_reference_v<T>, "opheap::property cannot wrap references");

public:
    using value_type = T;

    property() = default;
    property(const T& value) : value_(value) {}
    property(T&& value) : value_(std::move(value)) {}

    property(const property& other) : value_(other.value_) {}
    property(property&& other) noexcept(std::is_nothrow_move_constructible_v<T>)
        : value_(std::move(other.value_)) {}

    property& operator=(const property& other) {
        if (this != &other) assign(other.value_);
        return *this;
    }

    property& operator=(property&& other) noexcept(std::is_nothrow_move_assignable_v<T>) {
        if (this != &other) {
            value_ = std::move(other.value_);
            detail::bind_if_supported(value_, observer_binding());
            notify(change_kind::value, 0, sizeof(T));
        }
        return *this;
    }

    property& operator=(const T& value) {
        assign(value);
        return *this;
    }

    property& operator=(T&& value) {
        if constexpr (std::equality_comparable<T>) {
            if (value_ == value) return *this;
        }
        value_ = std::move(value);
        detail::bind_if_supported(value_, observer_binding());
        notify(change_kind::value, 0, sizeof(T));
        return *this;
    }

    [[nodiscard]] const T& get() const noexcept { return value_; }
    [[nodiscard]] const T* operator->() const noexcept { return &value_; }
    [[nodiscard]] operator const T&() const noexcept { return value_; }

    void bind(binding b) noexcept {
        observable::bind(b);
        detail::bind_if_supported(value_, b);
    }

    template<class F>
    decltype(auto) mutate(F&& function) {
        if constexpr (std::is_void_v<std::invoke_result_t<F, T&>>) {
            std::invoke(std::forward<F>(function), value_);
            detail::bind_if_supported(value_, observer_binding());
            notify(change_kind::value, 0, sizeof(T));
        } else {
            decltype(auto) result = std::invoke(std::forward<F>(function), value_);
            detail::bind_if_supported(value_, observer_binding());
            notify(change_kind::value, 0, sizeof(T));
            return result;
        }
    }

    property& operator++() requires requires(T x) { ++x; } {
        ++value_;
        notify(change_kind::value, 0, sizeof(T));
        return *this;
    }

    auto operator++(int) requires std::copy_constructible<T> && requires(T x) { x++; } {
        auto old = value_;
        value_++;
        notify(change_kind::value, 0, sizeof(T));
        return old;
    }

    template<class U>
    property& operator+=(U&& rhs) requires requires(T x, U y) { x += y; } {
        value_ += std::forward<U>(rhs);
        notify(change_kind::value, 0, sizeof(T));
        return *this;
    }

    template<class U>
    property& operator-=(U&& rhs) requires requires(T x, U y) { x -= y; } {
        value_ -= std::forward<U>(rhs);
        notify(change_kind::value, 0, sizeof(T));
        return *this;
    }

    friend bool operator==(const property&, const property&) requires std::equality_comparable<T> = default;

private:
    void assign(const T& value) {
        if constexpr (std::equality_comparable<T>) {
            if (value_ == value) return;
        }
        value_ = value;
        detail::bind_if_supported(value_, observer_binding());
        notify(change_kind::value, 0, sizeof(T));
    }

    T value_{};
};

} // namespace opheap
