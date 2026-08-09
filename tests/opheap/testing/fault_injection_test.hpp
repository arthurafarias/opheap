#pragma once
#include "test_group.hpp"
#include "test_support.hpp"

#include <memory>
#include <optional>

namespace opheap::testing {

struct fault_plan {
    bool partial_append_once{};
    bool fail_flush_data_once{};
    bool fail_replace_once{};
};

class fault_file final : public storage_file {
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

class fault_backend final : public storage_backend {
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

inline static test_group fault_injection_test{"fault_injection", {
    {"partial commit append is recovered as uncommitted tail", [](test_context& ctx) {
        auto directory = temporary_directory("fault-append");
        auto plan = std::make_shared<fault_plan>();
        plan->partial_append_once = true;
        auto backend = std::make_shared<fault_backend>(make_default_storage_backend(), plan);
        {
            auto heap = opheap::heap::open({.path = directory}, backend);
            auto tx = heap.begin();
            tx.object_root()["must_not_publish"] = true;
            ctx.throws<storage_error>([&] { tx.commit(); });
        }
        auto recovered = opheap::heap::open({.path = directory});
        auto tx = recovered.begin();
        ctx.check(tx.root().is_null());
    }},
    {"failed durability barrier poisons live heap", [](test_context& ctx) {
        auto directory = temporary_directory("fault-flush");
        auto plan = std::make_shared<fault_plan>();
        plan->fail_flush_data_once = true;
        auto backend = std::make_shared<fault_backend>(make_default_storage_backend(), plan);
        auto heap = opheap::heap::open({.path = directory}, backend);
        auto tx = heap.begin();
        tx.object_root()["n"] = 1;
        ctx.throws<storage_error>([&] { tx.commit(); });
        auto next = heap.begin();
        next.object_root("other")["n"] = 2;
        ctx.throws<storage_error>([&] { next.commit(); });
    }},
    {"interrupted checkpoint leaves journal recovery path", [](test_context& ctx) {
        auto directory = temporary_directory("fault-checkpoint");
        auto plan = std::make_shared<fault_plan>();
        auto backend = std::make_shared<fault_backend>(make_default_storage_backend(), plan);
        {
            auto heap = opheap::heap::open({.path = directory}, backend);
            auto tx = heap.begin(); tx.object_root()["n"] = 77; tx.commit();
            plan->fail_replace_once = true;
            ctx.throws<storage_error>([&] { heap.checkpoint(); });
        }
        auto recovered = opheap::heap::open({.path = directory});
        auto read = recovered.begin();
        ctx.equal(read.object_root().at("n").as_integer(), std::int64_t{77});
    }},
}};

} // namespace opheap::testing
