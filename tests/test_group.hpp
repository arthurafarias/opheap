#pragma once

#include <exception>
#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace opheap::testing {

class test_failure : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class test_context {
public:
    void check(bool condition, std::string_view message = "check failed") const {
        if (!condition) throw test_failure(std::string{message});
    }

    template<class L, class R>
    void equal(const L& lhs, const R& rhs, std::string_view message = "values differ") const {
        if (!(lhs == rhs)) {
            std::ostringstream stream;
            stream << message;
            throw test_failure(stream.str());
        }
    }

    template<class Exception, class F>
    void throws(F&& function, std::string_view message = "expected exception was not thrown") const {
        try {
            std::forward<F>(function)();
        } catch (const Exception&) {
            return;
        }
        throw test_failure(std::string{message});
    }
};

struct test_case {
    std::string name;
    std::function<void(test_context&)> run;
};

class test_group;

inline std::vector<const test_group*>& registry() {
    static std::vector<const test_group*> groups;
    return groups;
}

class test_group {
public:
    test_group(std::string name, std::initializer_list<test_case> tests)
        : name_(std::move(name)), tests_(tests) {
        registry().push_back(this);
    }

    [[nodiscard]] std::string_view name() const noexcept { return name_; }
    [[nodiscard]] const std::vector<test_case>& tests() const noexcept { return tests_; }

private:
    std::string name_;
    std::vector<test_case> tests_;
};

inline int run_all() {
    std::size_t passed = 0;
    std::size_t failed = 0;
    test_context context;
    for (const auto* group : registry()) {
        for (const auto& test : group->tests()) {
            try {
                test.run(context);
                ++passed;
                std::cout << "[PASS] " << group->name() << "::" << test.name << '\n';
            } catch (const std::exception& error) {
                ++failed;
                std::cerr << "[FAIL] " << group->name() << "::" << test.name
                          << " - " << error.what() << '\n';
            } catch (...) {
                ++failed;
                std::cerr << "[FAIL] " << group->name() << "::" << test.name
                          << " - unknown exception\n";
            }
        }
    }
    std::cout << "\n" << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}

} // namespace opheap::testing
