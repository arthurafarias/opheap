#pragma once

#include "fault_file.hpp"
#include "fault_plan.hpp"

#include <opheap/opheap.hpp>

#include <filesystem>
#include <memory>

namespace opheap::testing {

struct fault_backend final : public storage_backend {
public:
    fault_backend(std::shared_ptr<storage_backend> inner, std::shared_ptr<fault_plan> plan)
        : inner_(std::move(inner)), plan_(std::move(plan)) {}

    std::unique_ptr<storage_file> open_file(const std::filesystem::path& path, bool create) override {
        return std::make_unique<fault_file>(inner_->open_file(path, create), plan_);
    }
    void create_directories(const std::filesystem::path& path) override { inner_->create_directories(path); }
    bool exists(const std::filesystem::path& path) const override { return inner_->exists(path); }
    void atomic_replace(const std::filesystem::path& from, const std::filesystem::path& to) override {
        if (plan_->fail_replace_once) {
            plan_->fail_replace_once = false;
            throw storage_error("injected atomic replace failure");
        }
        inner_->atomic_replace(from, to);
    }
    void remove_file(const std::filesystem::path& path) override { inner_->remove_file(path); }
    void sync_directory(const std::filesystem::path& path) override { inner_->sync_directory(path); }

private:
    std::shared_ptr<storage_backend> inner_;
    std::shared_ptr<fault_plan> plan_;
};

} // namespace opheap::testing
