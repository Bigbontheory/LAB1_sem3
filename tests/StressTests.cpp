#include <gtest/gtest.h>

#include <algorithm>
#include <iterator>
#include <map>
#include <random>
#include <set>
#include <string>

#include "AVLTree/AVLTreeIO.hpp"
#include "LAB2_sem2/DynamicArray.hpp"
#include "Set/SetIO.hpp"

namespace {

template <typename T>
DynamicArray<T> shuffled_range(int count, int seed) {
    DynamicArray<T> result(count);
    for (int i = 0; i < count; ++i) result[i] = static_cast<T>(i);
    std::mt19937 random(seed);
    std::shuffle(result.begin(), result.end(), random);
    return result;
}

void expect_tree(const AVLTree<int, int>& tree, const std::map<int, int>& expected) {
    ASSERT_EQ(tree.size(), static_cast<int>(expected.size()));
    auto actual = tree.get_iterator();
    auto entry = expected.begin();
    while (actual->has_next()) {
        ASSERT_NE(entry, expected.end());
        EXPECT_EQ(actual->current().first, entry->first);
        EXPECT_EQ(actual->current().second, entry->second);
        actual->next();
        ++entry;
    }
    EXPECT_EQ(entry, expected.end());
    EXPECT_EQ((avl_io::from_string<int, int>(avl_io::to_string(tree)).size()), tree.size());
}

void expect_set(const Set<int>& actual, const std::set<int>& expected) {
    ASSERT_EQ(actual.size(), static_cast<int>(expected.size()));
    for (int key = -100; key <= 1100; ++key) {
        EXPECT_EQ(actual.contains(key), expected.contains(key)) << key;
    }
    EXPECT_TRUE(set_io::from_string<int>(set_io::to_string(actual)) == actual);
}

TEST(AVLStressTest, MixedOperationsMatchStdMapAndPreserveShape) {
    AVLTree<int, int> tree;
    std::map<int, int> reference;
    auto keys = shuffled_range<int>(1000, 744);
    for (int i = 0; i < keys.get_size(); ++i) {
        tree.insert(keys[i], i);
        reference[keys[i]] = i;
    }
    expect_tree(tree, reference);

    std::mt19937 random(881);
    std::uniform_int_distribution<int> key_distribution(-100, 1099);
    for (int step = 0; step < 3000; ++step) {
        int key = key_distribution(random);
        switch (step % 3) {
            case 0:
                tree.insert(key, step);
                reference[key] = step;
                break;
            case 1:
                tree.erase(key);
                reference.erase(key);
                break;
            case 2:
                ASSERT_EQ(tree.contains(key), reference.contains(key));
                if (reference.contains(key)) EXPECT_EQ(tree.get(key), reference.at(key));
                break;
        }
        if (step % 250 == 0) expect_tree(tree, reference);
    }
    expect_tree(tree, reference);
    for (int key : keys) tree.erase(key);
    for (int key : keys) reference.erase(key);
    expect_tree(tree, reference);
}

TEST(AVLStressTest, SortedInsertAndReverseErase) {
    AVLTree<int, int> tree;
    for (int key = 0; key < 2000; ++key) tree.insert(key, key * 2);
    ASSERT_EQ(tree.size(), 2000);
    for (int key = 1999; key >= 0; --key) {
        EXPECT_EQ(tree.get(key), key * 2);
        tree.erase(key);
        EXPECT_EQ(tree.size(), key);
    }
    EXPECT_EQ(avl_io::to_string(tree), "#");
}

TEST(AVLStressTest, MergeOverwritesDuplicatesWithoutChangingInputs) {
    AVLTree<int, int> left;
    AVLTree<int, int> right;
    for (int key = 0; key < 500; ++key) left.insert(key, key);
    for (int key = 250; key < 750; ++key) right.insert(key, -key);
    auto merged = left.merge(right);
    ASSERT_EQ(merged.size(), 750);
    for (int key = 0; key < 750; ++key) {
        EXPECT_EQ(merged.get(key), key < 250 ? key : -key);
    }
    EXPECT_EQ(left.get(300), 300);
    EXPECT_EQ(right.get(300), -300);
    EXPECT_EQ((avl_io::from_string<int, int>(avl_io::to_string(merged)).size()), 750);
}

TEST(SetStressTest, MixedOperationsMatchStdSet) {
    Set<int> actual;
    std::set<int> reference;
    std::mt19937 random(103);
    std::uniform_int_distribution<int> keys(-100, 1099);
    for (int step = 0; step < 3000; ++step) {
        const int key = keys(random);
        if (step % 2 == 0) {
            actual.insert(key);
            reference.insert(key);
        } else {
            actual.erase(key);
            reference.erase(key);
        }
        if (step % 300 == 0) expect_set(actual, reference);
    }
    expect_set(actual, reference);
}

TEST(SetStressTest, BinaryOperationsMatchStdSetAlgorithms) {
    Set<int> left;
    Set<int> right;
    std::set<int> first;
    std::set<int> second;
    std::mt19937 random(91);
    std::uniform_int_distribution<int> keys(-100, 1099);
    for (int i = 0; i < 1500; ++i) {
        int a = keys(random);
        int b = keys(random);
        left.insert(a);
        right.insert(b);
        first.insert(a);
        second.insert(b);
    }
    std::set<int> expected_union, expected_intersection, expected_difference;
    std::set_union(first.begin(), first.end(), second.begin(), second.end(),
                   std::inserter(expected_union, expected_union.end()));
    std::set_intersection(first.begin(), first.end(), second.begin(), second.end(),
                          std::inserter(expected_intersection, expected_intersection.end()));
    std::set_difference(first.begin(), first.end(), second.begin(), second.end(),
                        std::inserter(expected_difference, expected_difference.end()));
    expect_set(left.set_union(right), expected_union);
    expect_set(left.set_intersection(right), expected_intersection);
    expect_set(left.set_difference(right), expected_difference);
    EXPECT_EQ(left.is_subset_of(right), std::includes(second.begin(), second.end(),
                                                      first.begin(), first.end()));
    expect_set(left, first);
    expect_set(right, second);
}

}
