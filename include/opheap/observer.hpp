#pragma once

#include "opheap/types.hpp"

#include <cstddef>
#include <cstdint>

namespace opheap {

enum class change_kind : std::uint8_t {
    value,
    structure,
    allocation,
    deallocation
};

struct change_event {
    root_token root{};
    change_kind kind{change_kind::value};
    std::size_t offset{};
    std::size_t size{};
};

class change_observer {
public:
    virtual ~change_observer() = default;
    virtual void changed(const change_event& event) noexcept = 0;
};

struct binding {
    change_observer* observer{};
    root_token root{};
};

namespace detail {

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

} // namespace detail

class observable {
public:
    void bind(binding b) noexcept { binding_ = b; }

protected:
    [[nodiscard]] binding observer_binding() const noexcept { return binding_; }

    void notify(change_kind kind = change_kind::value,
                std::size_t offset = 0,
                std::size_t size = 0) const noexcept {
        if (binding_.observer != nullptr) {
            binding_.observer->changed({binding_.root, kind, offset, size});
        }
    }

private:
    binding binding_{};
};

} // namespace opheap
