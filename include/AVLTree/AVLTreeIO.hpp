#pragma once

#include <array>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

#include "AVLTree.hpp"

namespace avl_io {

inline constexpr std::string_view default_format = "{N}(L)[R]";

namespace detail {

struct Field {
    char part;
    char open;
    char close;
};

inline std::array<Field, 3> parse_format(std::string_view format) {
    std::array<Field, 3> fields{};
    const auto& locale = std::locale::classic();
    std::size_t pos = 0;
    std::string seen;
    for (auto& field : fields) {
        while (pos < format.size() && std::isspace(format[pos], locale)) {
            ++pos;
        }
        if (pos == format.size()) {
            throw std::invalid_argument("Expected three traversal fields");
        }
        field.open = format[pos++];

        if (format.substr(pos, 2) == "\xD0\x9A") { // К
            field.part = 'N';
            pos += 2;
        } else if (format.substr(pos, 2) == "\xD0\x9B") { // Л
            field.part = 'L';
            pos += 2;
        } else if (format.substr(pos, 2) == "\xD0\x9F") { // П
            field.part = 'R';
            pos += 2;
        } else if (pos < format.size()) {
            field.part = format[pos++];
        }

        if (pos == format.size() || std::string_view("NLR").find(field.part) == std::string_view::npos
            || seen.find(field.part) != std::string::npos) {
            throw std::invalid_argument("Expected N, L and R exactly once");
        }
        seen += field.part;
        field.close = format[pos++];

        for (char delimiter : {field.open, field.close}) {
            if (!std::ispunct(delimiter, locale) || delimiter == '#' || delimiter == '"'
                || delimiter == '\\') {
                throw std::invalid_argument("Invalid traversal delimiter");
            }
        }
    }
    while (pos < format.size() && std::isspace(format[pos], locale)) {
        ++pos;
    }
    if (pos != format.size()) {
        throw std::invalid_argument("Unexpected text after traversal format");
    }
    return fields;
}

template <typename T>
std::string encode(const T& value) {
    std::ostringstream output;
    output.imbue(std::locale::classic());
    if constexpr (std::is_floating_point_v<T>) {
        output.precision(std::numeric_limits<T>::max_digits10);
    }
    if (!(output << value)) {
        throw std::runtime_error("Cannot write AVL node data");
    }
    return output.str();
}

template <typename T>
T read_value(std::istream& input) {
    input >> std::ws;
    std::string encoded;
    if (input.peek() != '"' || !(input >> std::quoted(encoded))) {
        throw std::invalid_argument("Expected quoted AVL node data");
    }
    if constexpr (std::is_same_v<T, std::string>) {
        return encoded;
    } else {
        std::istringstream value_input(encoded);
        value_input.imbue(std::locale::classic());
        if constexpr (std::is_same_v<T, char> || std::is_same_v<T, signed char>
                      || std::is_same_v<T, unsigned char>) {
            value_input >> std::noskipws;
        }
        T value{};
        if (!(value_input >> value)) {
            throw std::invalid_argument("Invalid AVL node data");
        }
        value_input >> std::ws;
        if (!value_input.eof()) {
            throw std::invalid_argument("Unexpected text in AVL node data");
        }
        return value;
    }
}

inline void expect(std::istream& input, char expected) {
    char actual;
    if (!(input >> actual) || actual != expected) {
        throw std::invalid_argument("Unexpected AVL tree delimiter");
    }
}

}

template <typename TKey, typename TElement>
struct Codec {
    using Tree = AVLTree<TKey, TElement>;

    static std::string write(const Tree& tree, std::string_view format) {
        const auto fields = detail::parse_format(format);
        std::ostringstream output;
        write_node(output, tree.root.get(), fields);
        return output.str();
    }

    static Tree read(std::string_view text, std::string_view format) {
        const auto fields = detail::parse_format(format);
        std::istringstream input{std::string(text)};
        input.imbue(std::locale::classic());

        Tree tree;
        tree.root = read_node(input, fields, tree.count, 0);
        input >> std::ws;
        if (!input.eof()) {
            throw std::invalid_argument("Unexpected text after AVL tree");
        }
        validate_order(tree.root.get(), nullptr, nullptr);
        return tree;
    }

private:
    using Node = typename Tree::AVLNode;
    using NodePtr = typename Tree::NodePtr;
    using Fields = std::array<detail::Field, 3>;

    static void write_node(std::ostream& output, const Node* node, const Fields& fields) {
        if (!node) {
            output << '#';
            return;
        }

        for (const auto& field : fields) {
            output << field.open;
            if (field.part == 'N') {
                output << std::quoted(detail::encode(node->key)) << ' '
                       << std::quoted(detail::encode(node->value));
            } else {
                const auto& child = field.part == 'L' ? node->left : node->right;
                write_node(output, child.get(), fields);
            }
            output << field.close;
        }
    }

    static NodePtr read_node(std::istream& input, const Fields& fields, int& count, int depth) {
        input >> std::ws;
        if (input.peek() == '#') {
            input.get();
            return nullptr;
        }
        // A valid AVL with an int-sized node count cannot reach this depth.
        if (depth >= 2 * std::numeric_limits<int>::digits) {
            throw std::invalid_argument("AVL tree is too deep");
        }

        NodePtr node;
        NodePtr left;
        NodePtr right;
        for (const auto& field : fields) {
            detail::expect(input, field.open);
            if (field.part == 'N') {
                auto key = detail::read_value<TKey>(input);
                auto value = detail::read_value<TElement>(input);
                node = std::make_unique<Node>(std::move(key), std::move(value));
            } else if (field.part == 'L') {
                left = read_node(input, fields, count, depth + 1);
            } else {
                right = read_node(input, fields, count, depth + 1);
            }
            detail::expect(input, field.close);
        }

        const int left_height = left ? left->height : 0;
        const int right_height = right ? right->height : 0;
        if (left_height - right_height > 1 || right_height - left_height > 1
            || count == std::numeric_limits<int>::max()) {
            throw std::invalid_argument("Invalid AVL tree balance or size");
        }
        node->height = 1 + std::max(left_height, right_height);
        node->left = std::move(left);
        node->right = std::move(right);
        ++count;
        return node;
    }

    static void validate_order(const Node* node, const TKey* lower, const TKey* upper) {
        if (!node) {
            return;
        }
        if ((lower && !(*lower < node->key)) || (upper && !(node->key < *upper))) {
            throw std::invalid_argument("Invalid AVL key order");
        }
        validate_order(node->left.get(), lower, std::addressof(node->key));
        validate_order(node->right.get(), std::addressof(node->key), upper);
    }
};

template <typename TKey, typename TElement>
std::string to_string(const AVLTree<TKey, TElement>& tree, std::string_view format = default_format) {
    return Codec<TKey, TElement>::write(tree, format);
}

template <typename TKey, typename TElement>
AVLTree<TKey, TElement> from_string(std::string_view text, std::string_view format = default_format) {
    return Codec<TKey, TElement>::read(text, format);
}

}
