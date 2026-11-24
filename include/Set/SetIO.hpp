#pragma once

#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>

#include "Set.hpp"

namespace set_io {
namespace detail {

template <typename T>
T decode_element(const std::string& encoded) {
    if constexpr (std::is_same_v<T, std::string>) {
        return encoded;
    } else {
        std::istringstream input(encoded);
        input.imbue(std::locale::classic());
        if constexpr (std::is_same_v<T, char> || std::is_same_v<T, signed char>
                      || std::is_same_v<T, unsigned char>) {
            input >> std::noskipws;
        }
        T element{};
        if (!(input >> element)) {
            throw std::invalid_argument("Invalid set element");
        }
        input >> std::ws;
        if (!input.eof()) {
            throw std::invalid_argument("Unexpected text in set element");
        }
        return element;
    }
}

} // namespace detail

template <typename T>
std::string to_string(const Set<T>& set) {
    struct WriteElement {
        std::ostringstream& output;
        bool first = true;

        void operator()(const T& element) {
            std::ostringstream encoded;
            encoded.imbue(std::locale::classic());
            if constexpr (std::is_floating_point_v<T>) {
                encoded.precision(std::numeric_limits<T>::max_digits10);
            }
            if (!(encoded << element)) {
                throw std::runtime_error("Cannot write set element");
            }

            if (!first) {
                output << ", ";
            }
            output << std::quoted(encoded.str());
            first = false;
        }
    };

    std::ostringstream output;
    output << '{';
    set.for_each(WriteElement{output});
    output << '}';
    return output.str();
}

template <typename T>
Set<T> from_string(const std::string& text) {
    std::istringstream input(text);
    input.imbue(std::locale::classic());
    char delimiter;
    if (!(input >> delimiter) || delimiter != '{') {
        throw std::invalid_argument("Expected '{' at the start of the set");
    }

    Set<T> result;
    input >> std::ws;
    if (input.peek() == '}') {
        input.get();
    } else {
        while (true) {
            input >> std::ws;
            std::string encoded;
            if (input.peek() != '"' || !(input >> std::quoted(encoded))) {
                throw std::invalid_argument("Expected a quoted set element");
            }
            result.insert(detail::decode_element<T>(encoded));

            if (!(input >> delimiter) || (delimiter != ',' && delimiter != '}')) {
                throw std::invalid_argument("Expected ',' or '}' after set element");
            }
            if (delimiter == '}') {
                break;
            }
        }
    }

    input >> std::ws;
    if (!input.eof()) {
        throw std::invalid_argument("Unexpected text after the set");
    }
    return result;
}

}