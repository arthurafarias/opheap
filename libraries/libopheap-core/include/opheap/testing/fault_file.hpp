#pragma once

#include "fault_plan.hpp"

#include <opheap/opheap.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

namespace opheap::testing {

struct fault_file final : public storage_file {
public:
    fault_file(std::unique_ptr<storage_file> inner, std::shared_ptr<fault_plan> plan)
        : inner_(std::move(inner)), plan_(std::move(plan)) {}

    std::uint64_t size() const override { return inner_->size(); }
    void read_exact(std::uint64_t offset, std::span<std::byte> out) const override {
        inner_->read_exact(offset, out);
    }
    void write_exact(std::uint64_t offset, std::span<const std::byte> data) override {
        inner_->write_exact(offset, data);
    }
    std::uint64_t append(std::span<const std::byte> data) override {
        if (plan_->partial_append_once && data.size() > 1) {
            plan_->partial_append_once = false;
            const auto offset = inner_->size();
            inner_->write_exact(offset, data.first(data.size() / 2));
            throw storage_error("injected partial append");
        }
        return inner_->append(data);
    }
    void truncate(std::uint64_t new_size) override { inner_->truncate(new_size); }
    void flush_data() override {
        if (plan_->fail_flush_data_once) {
            plan_->fail_flush_data_once = false;
            throw storage_error("injected flush_data failure");
        }
        inner_->flush_data();
    }
    void flush_all() override { inner_->flush_all(); }

private:
    std::unique_ptr<storage_file> inner_;
    std::shared_ptr<fault_plan> plan_;
};

} // namespace opheap::testing
