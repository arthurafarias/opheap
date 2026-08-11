#pragma once

#include <ostream>
#include <string_view>

namespace opheap::utils::serialization::json {

inline void write_string(std::ostream& output, std::string_view value) {
    static constexpr char hex[] = "0123456789abcdef";
    output.put('"');
    for (const unsigned char c : value) {
        switch (c) {
        case '"': output << "\\\""; break;
        case '\\': output << "\\\\"; break;
        case '\b': output << "\\b"; break;
        case '\f': output << "\\f"; break;
        case '\n': output << "\\n"; break;
        case '\r': output << "\\r"; break;
        case '\t': output << "\\t"; break;
        default:
            if (c < 0x20U) {
                output << "\\u00" << hex[c >> 4U] << hex[c & 0x0fU];
            } else {
                output.put(static_cast<char>(c));
            }
        }
    }
    output.put('"');
}

} // namespace opheap::utils::serialization::json
