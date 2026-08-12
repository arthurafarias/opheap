#pragma once
#include "test_group.hpp"
#include "test_support.hpp"

#include <opheap/codec.hpp>

namespace opheap::testing {

struct codec_test : public test_group {
    codec_test() : test_group("codec", {
    {"round trips universal tree", [](test_context& ctx) {
        value source;
        source["n"] = 42;
        source["s"] = "hello";
        source["a"].as_array().push_back(value{true});
        detail::writer writer;
        codec<value>::encode(writer, source);
        detail::reader reader{writer.data()};
        auto decoded = codec<value>::decode(reader, std::pmr::get_default_resource());
        ctx.check(reader.eof());
        ctx.equal(decoded.at("n").as_integer(), std::int64_t{42});
        ctx.equal(decoded.at("s").as_string().view(), std::string_view{"hello"});
        ctx.check(decoded.at("a").as_array()[0].as_bool());
    }},
    {"decode rejects an unknown value tag", [](test_context& ctx) {
        detail::writer writer;
        writer.scalar<std::uint8_t>(200);
        detail::reader reader{writer.data()};
        ctx.throws<corruption_error>([&] { (void)codec<value>::decode(reader, std::pmr::get_default_resource()); });
    }},
    {"decode rejects implausible array and object element counts", [](test_context& ctx) {
        detail::writer array_writer;
        array_writer.scalar(codec<value>::tag::array);
        array_writer.scalar<std::uint64_t>(1'000'000);
        detail::reader array_reader{array_writer.data()};
        ctx.throws<corruption_error>([&] { (void)codec<value>::decode(array_reader, std::pmr::get_default_resource()); });

        detail::writer object_writer;
        object_writer.scalar(codec<value>::tag::object);
        object_writer.scalar<std::uint64_t>(1'000'000);
        detail::reader object_reader{object_writer.data()};
        ctx.throws<corruption_error>([&] { (void)codec<value>::decode(object_reader, std::pmr::get_default_resource()); });
    }},
    {"decode rejects implausible generic vector and map element counts", [](test_context& ctx) {
        detail::writer vector_writer;
        vector_writer.scalar<std::uint64_t>(1'000'000);
        detail::reader vector_reader{vector_writer.data()};
        ctx.throws<corruption_error>([&] {
            (void)codec<vector<property<int>>>::decode(vector_reader, std::pmr::get_default_resource());
        });

        detail::writer map_writer;
        map_writer.scalar<std::uint64_t>(1'000'000);
        detail::reader map_reader{map_writer.data()};
        ctx.throws<corruption_error>([&] {
            (void)codec<map<std::string, property<int>>>::decode(map_reader, std::pmr::get_default_resource());
        });
    }},
    {"reader rejects an oversized string length and a truncated payload", [](test_context& ctx) {
        detail::writer string_writer;
        string_writer.scalar<std::uint64_t>(100);
        detail::reader string_reader{string_writer.data()};
        ctx.throws<corruption_error>([&] { (void)string_reader.string(); });

        detail::writer short_writer;
        short_writer.scalar<std::uint16_t>(1);
        detail::reader short_reader{short_writer.data()};
        ctx.throws<corruption_error>([&] { (void)short_reader.scalar<std::uint64_t>(); });
    }},
    }) {}
};

inline static codec_test codec_test_instance;

} // namespace opheap::testing
