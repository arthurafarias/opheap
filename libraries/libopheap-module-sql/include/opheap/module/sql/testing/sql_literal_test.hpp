#pragma once
#include <opheap/testing/test_group.hpp>
#include <opheap/testing/test_support.hpp>

#include <opheap/module/sql/coerce.hpp>
#include <opheap/module/sql/column_type_from_string.hpp>
#include <opheap/module/sql/compare_value_literal.hpp>
#include <opheap/module/sql/less_for_sort.hpp>
#include <opheap/module/sql/sql_error.hpp>
#include <opheap/module/sql/to_literal.hpp>

#include <opheap/value.hpp>

namespace opheap::testing {

struct sql_literal_test : public test_group {
    sql_literal_test() : test_group("sql_literal", {
    {"coerce rejects a literal whose type does not match the column", [](test_context& ctx) {
        using namespace opheap::module::sql;
        ctx.throws<sql_error>([] { (void)coerce(literal{std::string{"x"}}, column_type::integer, "a"); });
        ctx.throws<sql_error>([] { (void)coerce(literal{std::string{"x"}}, column_type::real, "a"); });
        ctx.throws<sql_error>([] { (void)coerce(literal{std::int64_t{1}}, column_type::text, "a"); });
        ctx.throws<sql_error>([] { (void)coerce(literal{std::int64_t{1}}, column_type::boolean, "a"); });
    }},
    {"coerce accepts NULL for every column type", [](test_context& ctx) {
        using namespace opheap::module::sql;
        for (auto type : {column_type::integer, column_type::real, column_type::text, column_type::boolean}) {
            const auto result = coerce(literal{std::monostate{}}, type, "a");
            ctx.check(std::holds_alternative<std::monostate>(result));
        }
    }},
    {"coerce rejects an unrecognized column type", [](test_context& ctx) {
        using namespace opheap::module::sql;
        ctx.throws<sql_error>([] { (void)coerce(literal{std::int64_t{1}}, static_cast<column_type>(99), "a"); });
    }},
    {"column_type_from_string rejects an unknown type name", [](test_context& ctx) {
        ctx.throws<opheap::module::sql::sql_error>([] {
            (void)opheap::module::sql::column_type_from_string("BOGUS");
        });
    }},
    {"compare_value_literal rejects incomparable types", [](test_context& ctx) {
        using namespace opheap::module::sql;
        const opheap::value text{"x"};
        ctx.throws<sql_error>([&] { (void)compare_value_literal(text, literal{std::int64_t{1}}); });
        const opheap::value integer{1};
        ctx.throws<sql_error>([&] { (void)compare_value_literal(integer, literal{std::string{"x"}}); });
    }},
    {"to_literal rejects a value with no SQL-representable type", [](test_context& ctx) {
        opheap::value array_value;
        array_value.as_array().push_back(opheap::value{1});
        ctx.throws<opheap::module::sql::sql_error>([&] { (void)opheap::module::sql::to_literal(array_value); });

        opheap::value object_value;
        object_value["k"] = 1;
        ctx.throws<opheap::module::sql::sql_error>([&] { (void)opheap::module::sql::to_literal(object_value); });
    }},
    {"less_for_sort treats NULLs as sorting last regardless of direction", [](test_context& ctx) {
        opheap::value null_value;
        opheap::value present{1};
        ctx.check(!opheap::module::sql::less_for_sort(null_value, present));
        ctx.check(opheap::module::sql::less_for_sort(present, null_value));
    }},
    }) {}
};

inline static sql_literal_test sql_literal_test_instance;

} // namespace opheap::testing
