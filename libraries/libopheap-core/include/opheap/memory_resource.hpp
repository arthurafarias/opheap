#pragma once

#include "opheap/observer.hpp"

#include <cstddef>
#include <memory_resource>

namespace opheap {

// Decorates an arbitrary PMR resource and reports allocation topology changes.
// It deliberately does not claim to observe writes to already-allocated bytes.
class observing_resource final : public std::pmr::memory_resource {
public:
    explicit observing_resource(
        std::pmr::memory_resource* upstream = std::pmr::get_default_resource(),
        binding b = {}) noexcept
        : upstream_(upstream), binding_(b) {}

    void bind(binding b) noexcept { binding_ = b; }
    [[nodiscard]] std::pmr::memory_resource* upstream() const noexcept { return upstream_; }

protected:
    void* do_allocate(std::size_t bytes, std::size_t alignment) override {
        void* memory = upstream_->allocate(bytes, alignment);
        if (binding_.observer) {
            binding_.observer->changed({binding_.root, change_kind::allocation, 0, bytes});
        }
        return memory;
    }

    void do_deallocate(void* memory, std::size_t bytes, std::size_t alignment) override {
        upstream_->deallocate(memory, bytes, alignment);
        if (binding_.observer) {
            binding_.observer->changed({binding_.root, change_kind::deallocation, 0, bytes});
        }
    }

    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
        if (this == &other) return true;
        const auto* rhs = dynamic_cast<const observing_resource*>(&other);
        return rhs != nullptr && upstream_->is_equal(*rhs->upstream_);
    }

private:
    std::pmr::memory_resource* upstream_;
    binding binding_;
};

} // namespace opheap
