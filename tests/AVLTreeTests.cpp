#include <gtest/gtest.h>
#include <string>
#include <initializer_list>
#include <utility>
#include "DynamicArray.hpp"
#include "AVLTree.hpp"
#include "AVLTreeIO.hpp"

TEST(AVLTreeIOTest, WritesAndReadsAllSixFormats) {
    AVLTree<int, std::string> tree;
    tree.insert(2, "two");
    tree.insert(1, "one");
    tree.insert(3, "three");

    struct Example {
        std::string_view format;
        std::string_view expected;
    };
    const Example examples[] = {
        {"{N}(L)[R]", R"txt({"2" "two"}({"1" "one"}(#)[#])[{"3" "three"}(#)[#]])txt"},
        {"{N}[R](L)", R"txt({"2" "two"}[{"3" "three"}[#](#)]({"1" "one"}[#](#)))txt"},
        {"(L){N}[R]", R"txt(((#){"1" "one"}[#]){"2" "two"}[(#){"3" "three"}[#]])txt"},
        {"(L)[R]{N}", R"txt(((#)[#]{"1" "one"})[(#)[#]{"3" "three"}]{"2" "two"})txt"},
        {"[R]{N}(L)", R"txt([[#]{"3" "three"}(#)]{"2" "two"}([#]{"1" "one"}(#)))txt"},
        {"[R](L){N}", R"txt([[#](#){"3" "three"}]([#](#){"1" "one"}){"2" "two"})txt"},
        {"{\xD0\x9A}(\xD0\x9B)[\xD0\x9F]", R"txt({"2" "two"}({"1" "one"}(#)[#])[{"3" "three"}(#)[#]])txt"},
    };

    for (const auto& example : examples) {
        SCOPED_TRACE(example.format);
        EXPECT_EQ(avl_io::to_string(tree, example.format), example.expected);
        const auto restored = avl_io::from_string<int, std::string>(example.expected, example.format);
        EXPECT_EQ(restored.size(), 3);
        EXPECT_EQ(avl_io::to_string(restored), avl_io::to_string(tree));
    }
}

TEST(AVLTreeIOTest, PreservesShapeAndEscapedDataAndRestoresHeights) {
    AVLTree<int, std::string> tree;
    tree.insert(4, "a \"quote\" and \\");
    tree.insert(2, "");
    tree.insert(6, "two\nlines");
    tree.insert(1, "{[()]#}");

    const auto text = avl_io::to_string(tree, "<L}!N!<R]");
    auto restored = avl_io::from_string<int, std::string>(text, "<L}!N!<R]");
    EXPECT_EQ(restored.size(), 4);
    EXPECT_EQ(avl_io::to_string(restored), avl_io::to_string(tree));

    restored.insert(0, "zero");
    tree.insert(0, "zero");
    EXPECT_EQ(avl_io::to_string(restored), avl_io::to_string(tree));
}

TEST(AVLTreeIOTest, SupportsEmptyTreeAndRejectsMalformedInput) {
    using Tree = AVLTree<int, int>;
    using avl_io::from_string;
    EXPECT_EQ(avl_io::to_string(Tree{}), "#");
    EXPECT_EQ((from_string<int, int>(" # ").size()), 0);
    EXPECT_THROW(avl_io::to_string(Tree{}, "{N}(L)[L]"), std::invalid_argument);

    for (std::string_view text : {"", "# trailing", R"({"1" "a"}(#)[#])",
                                 R"({"2" "0"}({"3" "0"}(#)[#])[#])",
                                 R"({"2" "0"}({"2" "0"}(#)[#])[#])",
                                 R"({"3" "0"}({"2" "0"}({"1" "0"}(#)[#])[#])[#])",
                                 R"({"1" "0"}(#)[#)"}) {
        SCOPED_TRACE(text);
        EXPECT_THROW((from_string<int, int>(text)), std::invalid_argument);
    }
}

namespace {

struct CheckTraversal {
    const int*& next;
    const int* end;

    void operator()(const int& key, const std::string& value) const {
        ASSERT_NE(next, end) << "Unexpected extra key " << key;
        EXPECT_EQ(key, *next);
        EXPECT_EQ(value, "value_" + std::to_string(*next));
        ++next;
    }
};

class AVLTreeTraversalTest : public ::testing::Test {
protected:
    AVLTree<int, std::string> tree;

    void SetUp() override {
        for (int key : {4, 2, 6, 1, 3, 5, 7}) {
            tree.insert(key, "value_" + std::to_string(key));
        }
    }

    void expect_traversal(const std::string& order, std::initializer_list<int> expected_keys) {
        auto next = expected_keys.begin();
        const auto& const_tree = tree;
        const_tree.traverse(order, CheckTraversal{next, expected_keys.end()});
        EXPECT_EQ(next, expected_keys.end());
    }
};

TEST_F(AVLTreeTraversalTest, NLR) {
    expect_traversal("NLR", {4, 2, 1, 3, 6, 5, 7});
}

TEST_F(AVLTreeTraversalTest, NRL) {
    expect_traversal("NRL", {4, 6, 7, 5, 2, 3, 1});
}

TEST_F(AVLTreeTraversalTest, LNR) {
    expect_traversal("LNR", {1, 2, 3, 4, 5, 6, 7});
}

TEST_F(AVLTreeTraversalTest, LRN) {
    expect_traversal("LRN", {1, 3, 2, 5, 7, 6, 4});
}

TEST_F(AVLTreeTraversalTest, RNL) {
    expect_traversal("RNL", {7, 6, 5, 4, 3, 2, 1});
}

TEST_F(AVLTreeTraversalTest, RLN) {
    expect_traversal("RLN", {7, 5, 6, 3, 1, 2, 4});
}

TEST_F(AVLTreeTraversalTest, RejectsInvalidOrderBeforeVisiting) {
    const auto unexpected_visit = [](const int&, const std::string&) {
        ADD_FAILURE() << "Invalid order must not visit any nodes";
    };
    for (const char* order : {"", "LN", "LLR", "LNX", "LNRR", "lnr"}) {
        SCOPED_TRACE(order);
        EXPECT_THROW(tree.traverse(order, unexpected_visit), std::invalid_argument);
    }
}

TEST_F(AVLTreeTraversalTest, EmptyTreeSkipsVisitor) {
    const AVLTree<int, std::string> empty;
    empty.traverse("LNR", [](const int&, const std::string&) {
        ADD_FAILURE() << "Empty tree must not visit any nodes";
    });
}

}

TEST(AVLTreeTest, MapTransformsValuesAndPreservesOriginal) {
    struct DoubleValue {
        int operator()(const int& value) const {
            return value * 2;
        }
    };

    AVLTree<int, int> tree;
    tree.insert(2, 10);
    tree.insert(1, 20);
    tree.insert(3, 30);

    auto mapped = tree.map(DoubleValue{});

    EXPECT_EQ(mapped.size(), 3);
    EXPECT_EQ(mapped.get(2), 20);
    EXPECT_EQ(mapped.get(1), 40);
    EXPECT_EQ(mapped.get(3), 60);

    EXPECT_EQ(tree.get(2), 10);
    EXPECT_EQ(tree.get(1), 20);
    EXPECT_EQ(tree.get(3), 30);

    mapped.get(2) = 100;
    EXPECT_EQ(tree.get(2), 10);
}

namespace {

int unexpected_map_call(const int& value) {
    ADD_FAILURE() << "The mapper must not be called for an empty tree";
    return value;
}

}

TEST(AVLTreeTest, MapEmptyTreeSkipsMapper) {
    const AVLTree<int, int> tree;

    const auto mapped = tree.map(unexpected_map_call);

    EXPECT_EQ(mapped.size(), 0);
    EXPECT_FALSE(mapped.get_iterator()->has_next());
}

TEST(AVLTreeTest, WhereKeepsMatchingPairsInIndependentTree) {
    struct Matches {
        bool operator()(const int& key, const int& value) const {
            return key >= 2 && value > 15;
        }
    };

    AVLTree<int, int> tree;
    tree.insert(1, 40);
    tree.insert(2, 10);
    tree.insert(3, 20);
    tree.insert(4, 30);

    auto filtered = tree.where(Matches{});

    EXPECT_EQ(filtered.size(), 2);
    EXPECT_FALSE(filtered.contains(1));
    EXPECT_FALSE(filtered.contains(2));
    EXPECT_EQ(filtered.get(3), 20);
    EXPECT_EQ(filtered.get(4), 30);

    filtered.get(3) = 99;
    EXPECT_EQ(tree.size(), 4);
    EXPECT_EQ(tree.get(1), 40);
    EXPECT_EQ(tree.get(2), 10);
    EXPECT_EQ(tree.get(3), 20);
    EXPECT_EQ(tree.get(4), 30);
}

namespace {

bool keep_all_pairs(const int&, const int&) {
    return true;
}

bool reject_all_pairs(const int&, const int&) {
    return false;
}

bool unexpected_where_call(const int&, const int&) {
    ADD_FAILURE() << "The predicate must not be called for an empty tree";
    return false;
}

}

TEST(AVLTreeTest, WhereKeepsAllPairs) {
    AVLTree<int, int> tree;
    tree.insert(2, 20);
    tree.insert(1, 10);

    const auto filtered = tree.where(keep_all_pairs);

    EXPECT_EQ(filtered.size(), 2);
    EXPECT_EQ(filtered.get(1), 10);
    EXPECT_EQ(filtered.get(2), 20);
}

TEST(AVLTreeTest, WhereRejectsAllPairs) {
    AVLTree<int, int> tree;
    tree.insert(1, 10);
    tree.insert(2, 20);

    const auto filtered = tree.where(reject_all_pairs);

    EXPECT_EQ(filtered.size(), 0);
    EXPECT_FALSE(filtered.get_iterator()->has_next());
    EXPECT_EQ(tree.size(), 2);
}

TEST(AVLTreeTest, WhereEmptyTreeSkipsPredicate) {
    const AVLTree<int, int> tree;

    const auto filtered = tree.where(unexpected_where_call);

    EXPECT_EQ(filtered.size(), 0);
    EXPECT_FALSE(filtered.get_iterator()->has_next());
}

namespace {

int sum_values(int total, const int&, const int& value) {
    return total + value;
}

int unexpected_reduce_call(int total, const int&, const int&) {
    ADD_FAILURE() << "The reducer must not be called for an empty tree";
    return total;
}

}

TEST(AVLTreeTest, ReduceSumsValuesFromInitialValue) {
    AVLTree<int, int> tree;
    tree.insert(2, 10);
    tree.insert(1, 20);
    tree.insert(3, 30);

    EXPECT_EQ(tree.reduce(7, sum_values), 67);
}

TEST(AVLTreeTest, ReduceProcessesPairsInAscendingKeyOrder) {
    struct AppendKey {
        std::string operator()(const std::string& keys, const int& key, const int&) const {
            return keys + std::to_string(key);
        }
    };

    AVLTree<int, int> tree;
    tree.insert(2, 20);
    tree.insert(3, 30);
    tree.insert(1, 10);

    EXPECT_EQ(tree.reduce(std::string{}, AppendKey{}), "123");
}

TEST(AVLTreeTest, ReduceEmptyTreeReturnsInitialValue) {
    const AVLTree<int, int> tree;

    EXPECT_EQ(tree.reduce(7, unexpected_reduce_call), 7);
}

TEST(AVLTreeTest, ExtractSubtreePreservesShapeAndCreatesIndependentCopy) {
    AVLTree<int, int> tree;
    for (int key : {4, 2, 6, 1, 3, 5, 7}) {
        tree.insert(key, key * 10);
    }

    auto subtree = tree.extract_subtree(2);
    EXPECT_EQ(subtree.size(), 3);
    EXPECT_EQ(avl_io::to_string(subtree), R"txt({"2" "20"}({"1" "10"}(#)[#])[{"3" "30"}(#)[#]])txt");
    EXPECT_EQ(tree.extract_subtree(7).size(), 1);
    EXPECT_EQ(avl_io::to_string(tree.extract_subtree(4)), avl_io::to_string(tree));
    EXPECT_THROW(tree.extract_subtree(99), std::out_of_range);

    subtree.get(2) = 99;
    subtree.erase(1);
    EXPECT_EQ(tree.get(2), 20);
    EXPECT_TRUE(tree.contains(1));
    EXPECT_EQ(tree.size(), 7);
}

TEST(AVLTreeTest, ContainsSubtreeRequiresExactMatch) {
    AVLTree<int, int> tree;
    for (int key : {4, 2, 6, 1, 3, 5, 7}) {
        tree.insert(key, key * 10);
    }

    EXPECT_TRUE(tree.contains_subtree(tree));
    EXPECT_TRUE(tree.contains_subtree(tree.extract_subtree(2)));
    EXPECT_TRUE(tree.contains_subtree(tree.extract_subtree(7)));

    auto changed = tree.extract_subtree(2);
    changed.get(1) = 99;
    EXPECT_FALSE(tree.contains_subtree(changed));
    changed.get(1) = 10;
    changed.erase(3);
    EXPECT_FALSE(tree.contains_subtree(changed));

    AVLTree<int, int> missing;
    missing.insert(99, 990);
    EXPECT_FALSE(tree.contains_subtree(missing));
    const AVLTree<int, int> empty;
    EXPECT_TRUE(tree.contains_subtree(empty));
    EXPECT_TRUE(empty.contains_subtree(empty));
    EXPECT_FALSE(empty.contains_subtree(tree));
}

TEST(AVLTreeTest, MergeUsesRightValueForDuplicateKeys) {
    AVLTree<int, std::string> left;
    left.insert(1, "left only");
    left.insert(2, "left value");
    AVLTree<int, std::string> right;
    right.insert(2, "right value");
    right.insert(3, "right only");

    auto merged = left.merge(right);
    EXPECT_EQ(merged.size(), 3);
    EXPECT_EQ(merged.get(1), "left only");
    EXPECT_EQ(merged.get(2), "right value");
    EXPECT_EQ(merged.get(3), "right only");

    merged.get(2) = "changed";
    EXPECT_EQ(left.size(), 2);
    EXPECT_EQ(right.size(), 2);
    EXPECT_EQ(left.get(2), "left value");
    EXPECT_EQ(right.get(2), "right value");
}

TEST(AVLTreeTest, MergeSupportsEmptyTreesAndSelfMerge) {
    const AVLTree<int, int> empty;
    AVLTree<int, int> tree;
    tree.insert(2, 20);
    tree.insert(1, 10);

    EXPECT_EQ(empty.merge(empty).size(), 0);
    const auto from_left = tree.merge(empty);
    const auto from_right = empty.merge(tree);
    const auto from_self = tree.merge(tree);
    EXPECT_EQ(from_left.size(), 2);
    EXPECT_EQ(from_right.size(), 2);
    EXPECT_EQ(from_self.size(), 2);
    for (int key : {1, 2}) {
        EXPECT_EQ(from_left.get(key), tree.get(key));
        EXPECT_EQ(from_right.get(key), tree.get(key));
        EXPECT_EQ(from_self.get(key), tree.get(key));
    }
}

TEST(AVLTreeTest, EmptyTreeState) {
    AVLTree<int, std::string> tree;

    EXPECT_EQ(tree.size(), 0);
    EXPECT_EQ(tree.capacity(), 0);
    EXPECT_FALSE(tree.contains(10));

    EXPECT_THROW(tree.get(10), std::out_of_range);
}

TEST(AVLTreeTest, InsertAndGet) {
    AVLTree<int, std::string> tree;

    tree.insert(10, "Ten");
    tree.insert(20, "Twenty");
    tree.insert(5, "Five");

    EXPECT_EQ(tree.size(), 3);
    EXPECT_TRUE(tree.contains(10));
    EXPECT_TRUE(tree.contains(20));
    EXPECT_TRUE(tree.contains(5));
    EXPECT_FALSE(tree.contains(100));

    EXPECT_EQ(tree.get(10), "Ten");
    EXPECT_EQ(tree.get(20), "Twenty");
    EXPECT_EQ(tree.get(5), "Five");
}

TEST(AVLTreeTest, ModifyViaGet) {
    AVLTree<int, std::string> tree;
    tree.insert(1, "One");

    tree.get(1) = "Uno";
    EXPECT_EQ(tree.get(1), "Uno");
}

TEST(AVLTreeTest, EraseLeafNode) {
    AVLTree<int, std::string> tree;
    tree.insert(10, "10");
    tree.insert(5, "5");
    tree.insert(15, "15");

    tree.erase(5);

    EXPECT_EQ(tree.size(), 2);
    EXPECT_FALSE(tree.contains(5));
    EXPECT_TRUE(tree.contains(10));
    EXPECT_TRUE(tree.contains(15));
}

TEST(AVLTreeTest, EraseNodeWithOneChild) {
    AVLTree<int, std::string> tree;
    tree.insert(10, "10");
    tree.insert(5, "5");
    tree.insert(3, "3");

    tree.erase(5);

    EXPECT_EQ(tree.size(), 2);
    EXPECT_FALSE(tree.contains(5));
    EXPECT_TRUE(tree.contains(3));
    EXPECT_EQ(tree.get(3), "3");
}

TEST(AVLTreeTest, EraseNodeWithTwoChildren) {
    AVLTree<int, std::string> tree;
    tree.insert(20, "20");
    tree.insert(10, "10");
    tree.insert(30, "30");
    tree.insert(5, "5");
    tree.insert(15, "15");

    tree.erase(10);

    EXPECT_EQ(tree.size(), 4);
    EXPECT_FALSE(tree.contains(10));
    EXPECT_TRUE(tree.contains(5));
    EXPECT_TRUE(tree.contains(15));
    EXPECT_TRUE(tree.contains(20));
    EXPECT_TRUE(tree.contains(30));
}

TEST(AVLTreeTest, SequentialInsertMaintainsBalance) {
    AVLTree<int, int> tree;

    for (int i = 1; i <= 100; ++i) {
        tree.insert(i, i * 10);
    }

    EXPECT_EQ(tree.size(), 100);
    for (int i = 1; i <= 100; ++i) {
        EXPECT_TRUE(tree.contains(i));
        EXPECT_EQ(tree.get(i), i * 10);
    }
}

TEST(AVLTreeTest, IteratorInOrderTraversal) {
    AVLTree<int, std::string> tree;

    tree.insert(50, "50");
    tree.insert(30, "30");
    tree.insert(70, "70");
    tree.insert(20, "20");
    tree.insert(40, "40");
    tree.insert(60, "60");
    tree.insert(80, "80");

    auto it = tree.get_iterator();
    DynamicArray<int> sorted_keys;

    while (it->has_next()) {
        int sz = sorted_keys.get_size();
        sorted_keys.resize(sz + 1);
        sorted_keys[sz] = it->current().first;
        it->next();
    }

    int expected_data[] = {20, 30, 40, 50, 60, 70, 80};
    DynamicArray<int> expected(expected_data, 7);

    EXPECT_EQ(sorted_keys, expected);
}

TEST(AVLTreeTest, IteratorTryGet) {
    AVLTree<int, std::string> tree;
    tree.insert(1, "One");
    tree.insert(2, "Two");

    auto it = tree.get_iterator();
    std::pair<int, std::string> current_val;

    EXPECT_TRUE(it->try_get(current_val));
    EXPECT_EQ(current_val.first, 1);
    EXPECT_EQ(current_val.second, "One");
    it->next();

    EXPECT_TRUE(it->try_get(current_val));
    EXPECT_EQ(current_val.first, 2);
    EXPECT_EQ(current_val.second, "Two");
    it->next();

    EXPECT_FALSE(it->try_get(current_val));
}

TEST(AVLTreeTest, IteratorEmptyExceptions) {
    AVLTree<int, std::string> tree;
    auto it = tree.get_iterator();

    EXPECT_FALSE(it->has_next());
    EXPECT_THROW(it->current(), std::out_of_range);
}

TEST(AVLTreeTest, CopyConstructorCreatesDeepCopy) {
    AVLTree<int, std::string> tree1;
    tree1.insert(10, "A");
    tree1.insert(20, "B");

    AVLTree<int, std::string> tree2(tree1);

    EXPECT_EQ(tree2.size(), 2);
    EXPECT_TRUE(tree2.contains(10));
    EXPECT_TRUE(tree2.contains(20));

    tree2.get(10) = "A_modified";
    tree2.insert(30, "C");

    EXPECT_EQ(tree1.get(10), "A");
    EXPECT_FALSE(tree1.contains(30));
    EXPECT_EQ(tree1.size(), 2);
    EXPECT_EQ(tree2.size(), 3);
}

TEST(AVLTreeTest, CopyAssignmentCreatesDeepCopy) {
    AVLTree<int, std::string> tree1;
    tree1.insert(1, "One");

    AVLTree<int, std::string> tree2;
    tree2.insert(99, "Ninety-Nine");

    tree2 = tree1;

    EXPECT_EQ(tree2.size(), 1);
    EXPECT_TRUE(tree2.contains(1));
    EXPECT_FALSE(tree2.contains(99));

    tree2.get(1) = "Uno";
    EXPECT_EQ(tree1.get(1), "One");
}

TEST(AVLTreeTest, MoveConstructorTransfersOwnership) {
    AVLTree<int, std::string> tree1;
    tree1.insert(10, "Ten");
    tree1.insert(20, "Twenty");

    AVLTree<int, std::string> tree2(std::move(tree1));

    EXPECT_EQ(tree2.size(), 2);
    EXPECT_TRUE(tree2.contains(10));
    EXPECT_TRUE(tree2.contains(20));

    EXPECT_EQ(tree1.size(), 0);
    EXPECT_FALSE(tree1.contains(10));
}

TEST(AVLTreeTest, MoveAssignmentTransfersOwnership) {
    AVLTree<int, std::string> tree1;
    tree1.insert(5, "Five");

    AVLTree<int, std::string> tree2;
    tree2.insert(100, "Hundred");

    tree2 = std::move(tree1);

    EXPECT_EQ(tree2.size(), 1);
    EXPECT_TRUE(tree2.contains(5));
    EXPECT_FALSE(tree2.contains(100));

    EXPECT_EQ(tree1.size(), 0);
}
