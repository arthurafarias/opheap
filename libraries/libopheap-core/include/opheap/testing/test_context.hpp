#pragma once

#include "test_failure.hpp"

#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace opheap::testing {

struct test_context {
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

} // namespace opheap::testing
