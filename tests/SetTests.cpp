#include <gtest/gtest.h>

#include <initializer_list>
#include <string>

#include "Set/SetIO.hpp"

namespace {

Set<int> make_set(std::initializer_list<int> elements) {
    Set<int> set;
    for (int element : elements) {
        set.insert(element);
    }
    return set;
}

void expect_elements(const Set<int>& actual, std::initializer_list<int> expected) {
    EXPECT_EQ(actual.size(), static_cast<int>(expected.size()));
    for (int element : expected) {
        EXPECT_TRUE(actual.contains(element)) << "Missing element: " << element;
    }
}

int parity(int element) {
    return element % 2;
}

bool is_even(int element) {
    return parity(element) == 0;
}

struct AppendElement {
    std::string operator()(const std::string& result, int element) const {
        return result + std::to_string(element);
    }
};

} // namespace

TEST(SetTest, InsertAndEraseMaintainUniqueElements) {
    auto set = make_set({2, 1, 2});
    expect_elements(set, {1, 2});

    set.erase(99);
    expect_elements(set, {1, 2});

    set.erase(2);
    expect_elements(set, {1});
    EXPECT_FALSE(set.contains(2));
}

TEST(SetTest, UnionCombinesElementsWithoutDuplicates) {
    const auto left = make_set({1, 2});
    const auto right = make_set({2, 3});

    auto combined = left.set_union(right);
    expect_elements(combined, {1, 2, 3});
    combined.erase(2);
    expect_elements(left, {1, 2});
    expect_elements(right, {2, 3});
}

TEST(SetTest, IntersectionKeepsCommonElements) {
    const auto left = make_set({1, 2});
    const auto right = make_set({2, 3});

    expect_elements(left.set_intersection(right), {2});
    expect_elements(left.set_intersection(Set<int>{}), {});
}

TEST(SetTest, DifferenceKeepsOnlyElementsFromTheLeft) {
    const auto left = make_set({1, 2});
    const auto right = make_set({2, 3});

    expect_elements(left.set_difference(right), {1});
    expect_elements(right.set_difference(left), {3});
    expect_elements(left.set_difference(left), {});
}

TEST(SetTest, SubsetAndEqualityCompareMembership) {
    const auto pair = make_set({1, 2});
    const auto larger = make_set({3, 2, 1});

    EXPECT_TRUE(pair.is_subset_of(larger));
    EXPECT_FALSE(larger.is_subset_of(pair));
    EXPECT_TRUE(Set<int>{}.is_subset_of(pair));
    EXPECT_TRUE(pair == make_set({2, 1}));
    EXPECT_FALSE(pair == make_set({1, 3}));
}

TEST(SetTest, MapCombinesEqualResults) {
    const auto source = make_set({1, 2, 3});

    expect_elements(source.map(parity), {0, 1});
    expect_elements(source, {1, 2, 3});
}

TEST(SetTest, WhereKeepsMatchingElements) {
    const auto source = make_set({1, 2, 3, 4});

    expect_elements(source.where(is_even), {2, 4});
    expect_elements(make_set({1, 3}).where(is_even), {});
    expect_elements(source, {1, 2, 3, 4});
}

TEST(SetTest, ReduceUsesInitialValueAndAscendingOrder) {
    const auto source = make_set({3, 1, 2, 2});

    EXPECT_EQ(source.reduce(std::string{"values="}, AppendElement{}), "values=123");
    EXPECT_EQ(Set<int>{}.reduce(std::string{"empty"}, AppendElement{}), "empty");
}

TEST(SetTest, StringFormatRoundTripsInAscendingOrder) {
    const auto source = make_set({3, -1, 2});

    EXPECT_EQ(set_io::to_string(source), R"({"-1", "2", "3"})");
    EXPECT_TRUE(set_io::from_string<int>(set_io::to_string(source)) == source);
    expect_elements(set_io::from_string<int>(R"( { "3", "2", "3" } )"), {2, 3});
    EXPECT_EQ(set_io::to_string(Set<int>{}), "{}");
    expect_elements(set_io::from_string<int>("{}"), {});
}

TEST(SetTest, StringFormatPreservesSpecialCharacters) {
    Set<std::string> source;
    source.insert("");
    source.insert("  a,b {c} \"d\" \\ ");
    source.insert("line\nbreak");

    EXPECT_TRUE(set_io::from_string<std::string>(set_io::to_string(source)) == source);
}

TEST(SetTest, StringFormatRejectsInvalidInput) {
    for (const std::string text : {"", R"({"1",})", R"({"1" "2"})",
                                   R"({"1x"})", R"({"1"})" " extra", R"({"1)"}) {
        EXPECT_THROW(set_io::from_string<int>(text), std::invalid_argument) << text;
    }
}
