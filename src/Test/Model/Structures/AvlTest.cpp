#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "src/Model/Structures/Avl/Avl.hpp"

// ----------------------------------------------------------------------------
// Mini-framework: CHECK registra falhas sem interromper o teste.
// ----------------------------------------------------------------------------
static int g_checks = 0;
static int g_failures = 0;

#define CHECK(cond)                                                         \
    do {                                                                    \
        ++g_checks;                                                         \
        if (!(cond)) {                                                      \
            ++g_failures;                                                   \
            std::cerr << "  FALHOU (linha " << __LINE__ << "): " #cond      \
                      << "\n";                                              \
        }                                                                   \
    } while (0)

#define RUN(test)                                                           \
    do {                                                                    \
        std::cout << "[ RUN ] " #test "\n";                                 \
        test();                                                             \
    } while (0)

// Altura máxima teórica de uma AVL com n nós: ~1,44 * log2(n + 2).
static bool heightWithinAvlBound(const AVLTree<int>& tree) {
    const double bound = 1.4405 * std::log2(static_cast<double>(tree.size()) + 2.0);
    return tree.height() <= bound;
}

// ----------------------------------------------------------------------------
// Testes
// ----------------------------------------------------------------------------

void testEmptyTree() {
    AVLTree<int> tree;
    CHECK(tree.empty());
    CHECK(tree.size() == 0);
    CHECK(tree.height() == 0);
    CHECK(!tree.contains(1));
    CHECK(tree.find(1) == nullptr);
    CHECK(tree.toSortedVector().empty());

    bool minThrew = false;
    try { tree.min(); } catch (const std::out_of_range&) { minThrew = true; }
    CHECK(minThrew);

    bool maxThrew = false;
    try { tree.max(); } catch (const std::out_of_range&) { maxThrew = true; }
    CHECK(maxThrew);
}

void testInsertAndDuplicates() {
    AVLTree<int> tree;
    CHECK(tree.insert(10));
    CHECK(tree.insert(5));
    CHECK(tree.insert(20));
    CHECK(tree.size() == 3);
    CHECK(!tree.empty());

    CHECK(!tree.insert(10));  // duplicata é rejeitada
    CHECK(tree.size() == 3);
}

void testFindAndContains() {
    AVLTree<int> tree;
    for (int v : {8, 3, 10, 1, 6}) tree.insert(v);

    CHECK(tree.contains(6));
    CHECK(!tree.contains(7));

    const int* found = tree.find(10);
    CHECK(found != nullptr);
    CHECK(found != nullptr && *found == 10);
    CHECK(tree.find(99) == nullptr);
}

void testMinMax() {
    AVLTree<int> tree;
    for (int v : {50, 20, 70, 10, 60, 90}) tree.insert(v);
    CHECK(tree.min() == 10);
    CHECK(tree.max() == 90);
}

void testRotationsKeepHeightMinimal() {
    // Cada sequência de 3 chaves força um caso de rotação diferente.
    // Após o rebalanceamento, a árvore deve ter altura 2.
    const std::vector<std::vector<int>> cases = {
        {1, 2, 3},  // RR -> rotação simples à esquerda
        {3, 2, 1},  // LL -> rotação simples à direita
        {3, 1, 2},  // LR -> rotação dupla
        {1, 3, 2},  // RL -> rotação dupla
    };

    for (const auto& keys : cases) {
        AVLTree<int> tree;
        for (int v : keys) tree.insert(v);
        CHECK(tree.height() == 2);
        CHECK((tree.toSortedVector() == std::vector<int>{1, 2, 3}));
    }
}

void testBalanceWithSequentialInsertion() {
    // Sem balanceamento, inserir em ordem geraria altura 1000.
    AVLTree<int> tree;
    for (int i = 1; i <= 1000; ++i) tree.insert(i);

    CHECK(tree.size() == 1000);
    CHECK(heightWithinAvlBound(tree));
    CHECK(tree.min() == 1);
    CHECK(tree.max() == 1000);
}

void testRemove() {
    AVLTree<int> tree;
    for (int v : {50, 30, 70, 20, 40, 60, 80, 35}) tree.insert(v);

    CHECK(!tree.remove(999));  // inexistente
    CHECK(tree.size() == 8);

    CHECK(tree.remove(20));    // folha
    CHECK(tree.remove(40));    // um filho (35)
    CHECK(tree.remove(50));    // dois filhos (raiz)

    CHECK(tree.size() == 5);
    CHECK(!tree.contains(20));
    CHECK(!tree.contains(40));
    CHECK(!tree.contains(50));
    CHECK((tree.toSortedVector() == std::vector<int>{30, 35, 60, 70, 80}));
}

void testRemoveTriggersRebalance() {
    AVLTree<int> tree;
    for (int i = 1; i <= 200; ++i) tree.insert(i);

    // Remove toda a metade inferior: força várias rotações na remoção.
    for (int i = 1; i <= 100; ++i) CHECK(tree.remove(i));

    CHECK(tree.size() == 100);
    CHECK(tree.min() == 101);
    CHECK(heightWithinAvlBound(tree));
}

void testInOrderAndSortedVector() {
    AVLTree<int> tree;
    for (int v : {5, 2, 8, 1, 9, 3}) tree.insert(v);

    std::vector<int> visited;
    tree.inOrder([&visited](const int& key) { visited.push_back(key); });

    const std::vector<int> expected = {1, 2, 3, 5, 8, 9};
    CHECK(visited == expected);
    CHECK(tree.toSortedVector() == expected);
}

void testClearAndMove() {
    AVLTree<int> tree;
    for (int v : {1, 2, 3}) tree.insert(v);

    AVLTree<int> moved = std::move(tree);
    CHECK(moved.size() == 3);
    CHECK(moved.contains(2));

    moved.clear();
    CHECK(moved.empty());
    CHECK(moved.size() == 0);
    CHECK(moved.height() == 0);
}

void testStringKeys() {
    AVLTree<std::string> tree;
    for (const char* s : {"maca", "banana", "uva", "abacaxi"}) tree.insert(s);

    CHECK(tree.min() == "abacaxi");
    CHECK(tree.max() == "uva");
    CHECK(tree.contains("banana"));
    CHECK(tree.remove("maca"));
    CHECK(!tree.contains("maca"));
}

// ----------------------------------------------------------------------------

int main() {
    RUN(testEmptyTree);
    RUN(testInsertAndDuplicates);
    RUN(testFindAndContains);
    RUN(testMinMax);
    RUN(testRotationsKeepHeightMinimal);
    RUN(testBalanceWithSequentialInsertion);
    RUN(testRemove);
    RUN(testRemoveTriggersRebalance);
    RUN(testInOrderAndSortedVector);
    RUN(testClearAndMove);
    RUN(testStringKeys);

    std::cout << "\n" << (g_checks - g_failures) << "/" << g_checks
              << " verificacoes passaram.\n";
    return g_failures == 0 ? 0 : 1;
}
