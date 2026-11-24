#pragma once


template <typename TKey, typename TElement>
int AVLTree<TKey, TElement>::get_height(AVLNode* node) const noexcept {
    return node ? node-> height : 0;
}

template <typename TKey, typename TElement>
void AVLTree<TKey, TElement>:: update_height(AVLNode* node) noexcept {
    if (node) {
        node->height = 1 + std::max(get_height(node->left.get()), get_height(node->right.get()));
    }
}

template<typename TKey, typename TElement>
int AVLTree<TKey, TElement>::get_balance_factor(AVLNode* node) const noexcept {
    if (!node) return 0;
    return get_height(node->right.get()) - get_height(node->left.get());
}


template <typename TKey, typename TElement>
void AVLTree<TKey, TElement>::rotate_right(NodePtr& node){
    NodePtr pivot = std::move(node->left);
    node->left = std::move(pivot->right);

    AVLNode* old_root = node.get();
    pivot->right = std::move(node);

    node = std::move(pivot);

    update_height(old_root);
    update_height(node.get());

}

template<typename TKey, typename TElement>
void AVLTree<TKey, TElement>::rotate_left(NodePtr& node) {
    NodePtr pivot = std::move(node->right);
    node->right = std::move(pivot->left);

    AVLNode* old_root = node.get();
    pivot->left = std::move(node);
    node = std::move(pivot);

    update_height(old_root);
    update_height(node.get());
}


template<typename TKey, typename TElement>
void AVLTree<TKey, TElement>::balance(NodePtr& node) {
    if (!node) {
        return;
    }

    update_height(node.get());
    int bf = get_balance_factor(node.get());

    if (bf == -2) {
        if (get_balance_factor(node->left.get()) > 0) {
            rotate_left(node->left);
        }
        rotate_right(node);
    }
    else if (bf == 2) {
        if (get_balance_factor(node->right.get()) < 0) {
            rotate_right(node->right);
        }
        rotate_left(node);
    }
}

template <typename TKey,typename TElement>
void AVLTree<TKey, TElement>::insert(NodePtr& node, const TKey& key, const TElement& value) {
    if (!node) {
        node = std::make_unique<AVLNode>(key, value);
        ++count;
        return;
    }

    if (key < node->key) {
        insert(node->left, key, value);
    } else if (key > node->key) {
        insert(node->right, key, value);
    } else if (key == node->key) {
        node->value = value;
        return;
    }
    balance(node);
}

template<typename TKey, typename TElement>
auto AVLTree<TKey, TElement>::find_min(AVLNode* node ) const -> AVLNode* {
    if (!node) return nullptr;
    while (node->left) {
        node = node->left.get();
    }
    return node;
}

template<typename TKey, typename TElement>
void AVLTree<TKey, TElement>::erase(NodePtr& node, const TKey& key) {
    if (!node) return;

    if (key < node->key) {
        erase(node->left, key);
    } else if (key > node->key) {
        erase(node->right, key);
    } else {

        if (!node->left) {
            node = std::move(node->right);
            --count;
        }
        else if (!node->right) {
            node = std::move(node->left);
            --count;
        }
        else {
            AVLNode* min_node = find_min(node->right.get());
            node->key = min_node->key;
            node->value = min_node->value;
            erase(node->right, min_node->key);
        }
    }

    if (node) {
        balance(node);
    }
}

template<typename TKey, typename TElement>
auto AVLTree<TKey, TElement>::find_node(const TKey& key) const -> AVLNode* {
    AVLNode* curr = root.get();
    while (curr) {
        if (key < curr->key) {
            curr = curr->left.get();
        } else if (key > curr-> key) {
            curr = curr->right.get();
        } else if (key == curr->key) {
            return curr;
        }
    }
    return nullptr;
}



template <typename TKey, typename TElement>
auto AVLTree<TKey, TElement>::clone(const AVLNode* node, int* copied_count) const -> NodePtr {
    if (!node) return nullptr;
    auto new_node = std::make_unique<AVLNode>(node->key, node->value);
    new_node->height = node->height;
    new_node->left = clone(node->left.get(), copied_count);
    new_node->right = clone(node->right.get(), copied_count);
    if (copied_count) {
        ++*copied_count;
    }
    return new_node;
}

template <typename TKey, typename TElement>
template <typename TVisitor>
void AVLTree<TKey, TElement>::traverse(const std::string& order, TVisitor visitor) const {
    const auto node = order.find('N');
    const auto left = order.find('L');
    const auto right = order.find('R');
    if (order.size() != 3 || node == std::string::npos ||
        left == std::string::npos || right == std::string::npos) {
        throw std::invalid_argument("Expected N, L and R exactly once (e.g. LNR)");
    }

    auto position = VisitPosition::BetweenChildren;
    if (node == 0) {
        position = VisitPosition::BeforeChildren;
    } else if (node == 2) {
        position = VisitPosition::AfterChildren;
    }
    const bool right_first = right < left;
    traverse_node(root.get(), right_first, position, visitor);
}

template <typename TKey, typename TElement>
template <typename TVisitor>
void AVLTree<TKey, TElement>::traverse_node(
    const AVLNode* node, bool right_first, VisitPosition position, TVisitor& visitor) const {
    if (!node) {
        return;
    }

    const auto& first = right_first ? node->right : node->left;
    const auto& second = right_first ? node->left : node->right;

    if (position == VisitPosition::BeforeChildren) {
        visitor(node->key, node->value);
    }
    traverse_node(first.get(), right_first, position, visitor);
    if (position == VisitPosition::BetweenChildren) {
        visitor(node->key, node->value);
    }
    traverse_node(second.get(), right_first, position, visitor);
    if (position == VisitPosition::AfterChildren) {
        visitor(node->key, node->value);
    }
}

template<typename TKey, typename TElement>
template<typename TMapper>
AVLTree<TKey, TElement> AVLTree<TKey, TElement>::map(TMapper mapper) const {
    AVLTree<TKey, TElement> result;
    result.root = map_node(root.get(), mapper);
    result.count = count;
    return result;
}

template<typename TKey, typename TElement>
template<typename TMapper>
auto AVLTree<TKey, TElement>::map_node(
    const AVLNode* node, TMapper& mapper) const -> NodePtr {
    if (!node) {
        return nullptr;
    }

    auto result = std::make_unique<AVLNode>(
        node->key, mapper(node->value));

    result->height = node->height;
    result->left = map_node(node->left.get(), mapper);
    result->right = map_node(node->right.get(), mapper);

    return result;
}

template <typename TKey, typename TElement>
template <typename TPredicate>
AVLTree<TKey, TElement> AVLTree<TKey, TElement>::where(TPredicate predicate) const {
    struct FilterVisitor {
        TPredicate& predicate;
        AVLTree<TKey, TElement>& result;

        void operator()(const TKey& key, const TElement& value) const {
            if (predicate(key, value)) {
                result.insert(key, value);
            }
        }
    };

    AVLTree<TKey, TElement> result;
    traverse("LNR", FilterVisitor{predicate, result});
    return result;
}

template <typename TKey, typename TElement>
template <typename TAccumulator, typename TReducer>
TAccumulator AVLTree<TKey, TElement>::reduce(TAccumulator initial, TReducer reducer) const {
    struct ReduceVisitor {
        TAccumulator& accumulator;
        TReducer& reducer;

        void operator()(const TKey& key, const TElement& value) const {
            accumulator = reducer(accumulator, key, value);
        }
    };

    traverse("LNR", ReduceVisitor{initial, reducer});
    return initial;
}

template <typename TKey, typename TElement>
auto AVLTree<TKey, TElement>::extract_subtree(const TKey& key) const -> AVLTree {
    const AVLNode* node = find_node(key);
    if (!node) {
        throw std::out_of_range("Key not found in Tree");
    }

    AVLTree result;
    result.root = clone(node, &result.count);
    return result;
}

template <typename TKey, typename TElement>
bool AVLTree<TKey, TElement>::contains_subtree(const AVLTree& other) const {
    if (!other.root) {
        return true;
    }
    return same_subtree(find_node(other.root->key), other.root.get());
}

template <typename TKey, typename TElement>
auto AVLTree<TKey, TElement>::merge(const AVLTree& other) const -> AVLTree {
    struct InsertPair {
        AVLTree& result;

        void operator()(const TKey& key, const TElement& value) const {
            result.insert(key, value);
        }
    };

    AVLTree result(*this);
    other.traverse("LNR", InsertPair{result});
    return result;
}

template <typename TKey, typename TElement>
bool AVLTree<TKey, TElement>::same_subtree(const AVLNode* first, const AVLNode* second) {
    if (!first || !second) {
        return first == second;
    }

    return first->key == second->key && first->value == second->value
        && same_subtree(first->left.get(), second->left.get())
        && same_subtree(first->right.get(), second->right.get());
}



