#pragma once

#include<memory>
#include<utility>
#include<stdexcept>
#include<algorithm>
#include <string>
#include "../Interface/IDictionary.hpp"
#include "../Interface/IIterator.hpp"
#include"../LAB2_sem2/Stack.hpp"

namespace avl_io {
template <typename TKey, typename TElement>
struct Codec;
}

template <typename TKey, typename TElement>
class AVLTree: public IDictionary<TKey, TElement> {
private:
    friend struct avl_io::Codec<TKey, TElement>;

    struct AVLNode {
        TKey key;
        TElement value;
        int height;
        std::unique_ptr<AVLNode> left;
        std::unique_ptr<AVLNode> right;

        AVLNode(const TKey& k, const TElement& val)
            : key(k), value(val), height(1), left(nullptr), right(nullptr) {}

        AVLNode(TKey&& k, TElement&& val)
            : key(std::move(k)), value(std::move(val)), height(1), left(nullptr), right(nullptr) {}
    };



    class AVLTreeIterator : public IIterator<std::pair<TKey, TElement>> {
    private:
        Stack<AVLNode*> stack_;

        void push_left_path(AVLNode* node) {
            while (node) {
                stack_.push(node);
                node = node->left.get();
            }
        }

    public:
        explicit AVLTreeIterator(AVLNode* root) {
            push_left_path(root);
        };

        bool has_next() const override {
            return !stack_.empty();
        }

        std::pair<TKey, TElement> current() const override {
            if (stack_.empty()) {
                throw std::out_of_range("Iterator is empty");
            }

            AVLNode* curr = stack_.top();
            return {curr->key, curr->value};
        }

        bool next() override {
            if (stack_.empty()) {
                return false;
            }

            AVLNode* curr = stack_.top();
            stack_.pop();

            if (curr -> right) {
                push_left_path(curr->right.get());
            }

            return !stack_.empty();

        }
        bool try_get(std::pair<TKey, TElement>& value) const override {
            if (stack_.empty()) {
                return false;
            }

            AVLNode* curr = stack_.top();
            value = {curr->key, curr->value};
            return true;
        }
    };

    using NodePtr = std::unique_ptr<AVLNode>;

    NodePtr root;
    int count = 0;

    auto clone(const AVLNode* node, int* copied_count = nullptr) const -> NodePtr;
    static bool same_subtree(const AVLNode* first, const AVLNode* second);
    int get_height(AVLNode* node) const noexcept;
    int get_balance_factor(AVLNode* node) const noexcept;
    void update_height(AVLNode* node) noexcept;

    void rotate_right(NodePtr& node);
    void rotate_left(NodePtr& node);
    void balance(NodePtr& node);
    auto find_min(AVLNode* node) const -> AVLNode*;

    auto find_node(const TKey& key) const -> AVLNode*;

    void insert(NodePtr& node, const TKey& key, const TElement& value);
    void erase(NodePtr& node, const TKey& key);

    enum class VisitPosition { BeforeChildren, BetweenChildren, AfterChildren };

    template <typename TVisitor>
    void traverse_node(const AVLNode* node, bool right_first,
                       VisitPosition position, TVisitor& visitor) const;


    template<typename TMapper>
    NodePtr map_node(const AVLNode* node, TMapper& mapper) const;


public:

    class NodeView {
    public:
        explicit operator bool() const noexcept { return node != nullptr; }
        const TKey& key() const { return node->key; }
        const TElement& value() const { return node->value; }
        NodeView left() const noexcept { return NodeView(node->left.get()); }
        NodeView right() const noexcept { return NodeView(node->right.get()); }

    private:
        friend class AVLTree;
        explicit NodeView(const AVLNode* node) noexcept : node(node) {}

        const AVLNode* node;
    };

    NodeView root_view() const noexcept { return NodeView(root.get()); }

    AVLTree() = default;
    AVLTree(const AVLTree& other)
        : root(clone(other.root.get())), count(other.count) {}

    AVLTree& operator=(const AVLTree& other) {
        if (this != &other) {
            root = clone(other.root.get());
            count = other.count;
        }
        return *this;
    }

    AVLTree(AVLTree&& other) noexcept
        : root(std::move(other.root)), count(other.count) {
        other.count = 0;
    }
    AVLTree& operator=(AVLTree&& other) noexcept {
        if (this!= &other) {
            root = std::move(other.root);
            count = other.count;
            other.count = 0;
        }
        return *this;
    }

    ~AVLTree() override = default;



    void insert (const TKey& key, const TElement& value) override {
        insert(root, key, value);
    }

    void erase(const TKey& key) override {
        erase(root, key);
    }

    int size() const override {
        return count;
    }

    int capacity() const override {
        return count;
    }

    bool contains(const TKey& key) const override {
        return find_node(key) != nullptr;
    }

    const TElement& get(const TKey& key) const override {
        AVLNode* node = find_node(key);

        if (!node) {
            throw std::out_of_range("Key not found in Tree");
        }
        return node->value;
    }

    TElement& get(const TKey& key)  override {
        AVLNode* node = find_node(key);

        if (!node) {
            throw std::out_of_range("Key not found in Tree");
        }

        return node->value;
    }

    template <typename TVisitor>
    void traverse(const std::string& order, TVisitor visitor) const;

    std::unique_ptr<IIterator<std::pair<TKey, TElement>>> get_iterator() const override  {
        return std::make_unique<AVLTreeIterator>(root.get());
    }

    template<typename TMapper>
    AVLTree<TKey, TElement> map(TMapper mapper) const;

    template <typename TPredicate>
    AVLTree<TKey, TElement> where(TPredicate predicate) const;

    template <typename TAccumulator, typename TReducer>
    TAccumulator reduce(TAccumulator initial, TReducer reducer) const;

    AVLTree extract_subtree(const TKey& key) const;
    bool contains_subtree(const AVLTree& other) const;
    AVLTree merge(const AVLTree& other) const;

};

#include "AVLTree.tpp"
