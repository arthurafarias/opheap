#pragma once

#include <atomic>
#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

#include <opheap/opheap.hpp>

namespace opheap::testing {

inline std::filesystem::path temporary_directory(std::string_view name) {
    static std::atomic<unsigned long long> counter{0};
    auto path = std::filesystem::temp_directory_path() /
        ("opheap-test-" + std::string{name} + "-" + std::to_string(counter.fetch_add(1)));
    std::filesystem::remove_all(path);
    std::filesystem::create_directories(path);
    return path;
}

class recording_observer final : public change_observer {
public:
    void changed(const change_event& event) noexcept override { events.push_back(event); }
    std::vector<change_event> events;
};

} // namespace opheap::testing
