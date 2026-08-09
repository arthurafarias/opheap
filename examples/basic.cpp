#include <opheap/opheap.hpp>

#include <filesystem>
#include <iostream>

int main() {
    const auto path = std::filesystem::temp_directory_path() / "opheap-basic";
    std::filesystem::remove_all(path);

    auto heap = opheap::heap::open({.path = path});
    {
        auto tx = heap.begin();
        auto& state = tx.object_root();
        state["users"]["arthur"]["age"] = 42;
        state["users"]["arthur"]["name"] = "Arthur";
        state["users"]["arthur"]["roles"].as_array().push_back(opheap::value{"admin"});
        tx.commit();
    }
    heap.close();

    auto reopened = opheap::heap::open({.path = path});
    auto tx = reopened.begin();
    const auto& state = tx.object_root();
    std::cout << state.at("users").at("arthur").at("name").as_string().view() << '\n';
}
