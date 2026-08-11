#pragma once

#include "opheap/binding.hpp"
#include "opheap/change_kind.hpp"

#include <cstddef>

namespace opheap {

struct observable {
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
