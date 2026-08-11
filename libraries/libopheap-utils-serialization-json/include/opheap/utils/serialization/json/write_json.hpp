#pragma once

#include "opheap/utils/serialization/json/write_string.hpp"

#include <opheap/value.hpp>

#include <charconv>
#include <ostream>
#include <stdexcept>

namespace opheap::utils::serialization::json {

inline void write_json(std::ostream& output, const opheap::value& value) {
    if (value.is_null()) {
        output << "null";
    } else if (value.is_bool()) {
        output << (value.as_bool() ? "true" : "false");
    } else if (value.is_integer()) {
        output << value.as_integer();
    } else if (value.is_number()) {
        char buffer[64];
        const auto [end, error] = std::to_chars(buffer, buffer + sizeof(buffer), value.as_number());
        if (error != std::errc{}) throw std::runtime_error("cannot format number");
        output.write(buffer, end - buffer);
    } else if (value.is_string()) {
        write_string(output, value.as_string().view());
    } else if (value.is_array()) {
        output.put('[');
        bool first = true;
        for (const auto& item : value.as_array()) {
            if (!first) output.put(',');
            first = false;
            write_json(output, item);
        }
        output.put(']');
    } else {
        output.put('{');
        bool first = true;
        for (const auto& [key, item] : value.as_object()) {
            if (!first) output.put(',');
            first = false;
            write_string(output, key);
            output.put(':');
            write_json(output, item);
        }
        output.put('}');
    }
}

} // namespace opheap::utils::serialization::json
