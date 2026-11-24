#pragma once

#include <utility>
#include <variant>

#include "../AVLTree/AVLTree.hpp"

template <typename T>
class Set {
public:
    void insert(const T& element) {
        tree_.insert(element, std::monostate{});
    }

    void erase(const T& element) {
        tree_.erase(element);
    }

    bool contains(const T& element) const {
        return tree_.contains(element);
    }

    int size() const {
        return tree_.size();
    }

    Set set_union(const Set& other) const {
        struct InsertVisitor {
            Set& result;

            void operator()(const T& element, const std::monostate&) const {
                result.insert(element);
            }
        };

        Set result = *this;
        other.tree_.traverse("LNR", InsertVisitor{result});
        return result;
    }

    Set set_intersection(const Set& other) const {
        return filter_membership(other, true);
    }

    Set set_difference(const Set& other) const {
        return filter_membership(other, false);
    }

    bool is_subset_of(const Set& other) const {
        if (size() > other.size()) {
            return false;
        }

        auto iterator = tree_.get_iterator();
        while (iterator->has_next()) {
            if (!other.contains(iterator->current().first)) {
                return false;
            }
            iterator->next();
        }
        return true;
    }

    bool operator==(const Set& other) const {
        return size() == other.size() && is_subset_of(other);
    }

    template <typename TMapper>
    Set map(TMapper mapper) const {
        struct MapVisitor {
            TMapper& mapper;
            Set& result;

            void operator()(const T& element, const std::monostate&) const {
                result.insert(mapper(element));
            }
        };

        Set result;
        tree_.traverse("LNR", MapVisitor{mapper, result});
        return result;
    }

    template <typename TPredicate>
    Set where(TPredicate predicate) const {
        struct FilterPredicate {
            TPredicate& predicate;

            bool operator()(const T& element, const std::monostate&) const {
                return static_cast<bool>(predicate(element));
            }
        };

        Set result;
        result.tree_ = tree_.where(FilterPredicate{predicate});
        return result;
    }

    template <typename TAccumulator, typename TReducer>
    TAccumulator reduce(TAccumulator initial, TReducer reducer) const {
        struct ReduceAdapter {
            TReducer& reducer;

            TAccumulator operator()(TAccumulator& accumulator, const T& element,
                                    const std::monostate&) const {
                return reducer(accumulator, element);
            }
        };

        return tree_.reduce(std::move(initial), ReduceAdapter{reducer});
    }

    template <typename TVisitor>
    void for_each(TVisitor visitor) const {
        struct ElementVisitor {
            TVisitor& visitor;

            void operator()(const T& element, const std::monostate&) const {
                visitor(element);
            }
        };

        tree_.traverse("LNR", ElementVisitor{visitor});
    }

private:

    Set filter_membership(const Set& other, bool keep_common) const {
        struct MatchesMembership {
            const Set& other;
            bool keep_common;

            bool operator()(const T& element) const {
                return other.contains(element) == keep_common;
            }
        };

        return where(MatchesMembership{other, keep_common});
    }

    AVLTree<T, std::monostate> tree_;
};
