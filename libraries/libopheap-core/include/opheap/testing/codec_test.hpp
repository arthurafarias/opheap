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
    }) {}
};

inline static codec_test codec_test_instance;

} // namespace opheap::testing
