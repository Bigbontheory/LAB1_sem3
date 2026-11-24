#include "AVLTree/AVLTree.hpp"
#include "LAB2_sem2/DynamicArray.hpp"
#include "Set/Set.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <numeric>
#include <random>
#include <stdexcept>

using Clock = std::chrono::steady_clock;
using Tree = AVLTree<int, int>;
using Times = std::array<double, 4>;

struct Counter {
    std::int64_t& sum;
    int& count;
    void operator()(int key) const { sum += key; ++count; }
    void operator()(int key, int) const { (*this)(key); }
};

void insert(Tree& tree, int key) { tree.insert(key, key); }
void insert(Set<int>& set, int key) { set.insert(key); }
void visit(const Tree& tree, Counter counter) { tree.traverse("LNR", counter); }
void visit(const Set<int>& set, Counter counter) { set.for_each(counter); }

double elapsed(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

template<class Container>
Times measure(const DynamicArray<int>& keys, const DynamicArray<int>& queries) {
    Container data;
    Times times{};
    const int n = keys.get_size();
    auto start = Clock::now();
    for (int key : keys) insert(data, key);
    times[0] = elapsed(start);
    if (data.size() != n) throw std::logic_error("insert count");

    int hits = 0;
    start = Clock::now();
    for (int key : queries) hits += data.contains(key);
    times[1] = elapsed(start);
    if (hits != n) throw std::logic_error("lookup count");

    std::int64_t sum = 0;
    int visited = 0;
    start = Clock::now();
    visit(data, Counter{sum, visited});
    times[2] = elapsed(start);
    if (visited != n || sum != std::int64_t(n) * (n - 1) / 2)
        throw std::logic_error("traversal result");

    start = Clock::now();
    for (int key : queries) data.erase(key);
    times[3] = elapsed(start);
    if (data.size() != 0) throw std::logic_error("erase count");
    return times;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: lab_benchmark output.csv\n";
        return 1;
    }
    std::ofstream output(argv[1]);
    output.exceptions(std::ios::failbit | std::ios::badbit);
    output << "container,n,trial,insert_ms,lookup_ms,traverse_ms,erase_ms\n";
    for (int n : {10000, 30000, 100000, 200000, 300000, 400000,
                  500000, 600000, 700000, 800000, 900000, 1000000}) {
        DynamicArray<int> keys(n);
        for (int trial = 0; trial < 3; ++trial) {
            std::iota(keys.begin(), keys.end(), 0);
            std::mt19937 random(4815 + trial);
            std::shuffle(keys.begin(), keys.end(), random);
            auto queries = keys;
            std::shuffle(queries.begin(), queries.end(), random);
            for (int i = 0; i < 2; ++i) {
                const bool tree = (trial + i) % 2 == 0;
                auto times = tree ? measure<Tree>(keys, queries) : measure<Set<int>>(keys, queries);
                output << (tree ? "AVL" : "Set") << ',' << n << ',' << trial + 1;
                for (double ms : times) output << ',' << ms;
                output << '\n';
            }
        }
    }
    output.close();
}
