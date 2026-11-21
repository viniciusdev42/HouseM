#include <iostream>
#include <stdexcept>
#include <vector>

#include "src/Main/Model/Structures/DigitalSearch/DigitalSearch.hpp"

// Usamos uint8_t nos testes para controlar facilmente os bits (MSB = 128).
using Tree = ::DigitalSearchTree::DigitalSearchTree<std::uint8_t>;
using DigitalSearchTree::TraversalOrder;

static int g_failures = 0;

#define CHECK(cond)                                                              \
    do {                                                                         \
        if (!(cond)) {                                                           \
            ++g_failures;                                                        \
            std::cout << "  FALHOU (linha " << __LINE__ << "): " #cond << "\n";  \
        }                                                                        \
    } while (0)

static std::vector<int> Collect(const Tree& tree, TraversalOrder order) {
    std::vector<int> keys;
    tree.Traverse([&keys](const std::uint8_t& k) { keys.push_back(k); }, order);
    return keys;
}

void TestGetBit() {
    CHECK(Tree::GetBit(128, 0) == true);   // MSB
    CHECK(Tree::GetBit(128, 1) == false);
    CHECK(Tree::GetBit(1, 7) == true);     // LSB
    CHECK(Tree::GetBit(1, 0) == false);

    bool threw = false;
    try { Tree::GetBit(1, 8); } catch (const std::out_of_range&) { threw = true; }
    CHECK(threw);
}

void TestEmptyTree() {
    Tree tree;
    CHECK(tree.Empty());
    CHECK(tree.Size() == 0);
    CHECK(!tree.Contains(10));
    CHECK(tree.Search(10) == nullptr);
    CHECK(!tree.Remove(10));
}

void TestInsertAndSearch() {
    Tree tree;
    CHECK(tree.Insert(128));
    CHECK(tree.Insert(64));
    CHECK(tree.Insert(192));
    CHECK(tree.Size() == 3);
    CHECK(!tree.Empty());

    CHECK(!tree.Insert(64));          // duplicada
    CHECK(tree.Size() == 3);

    CHECK(tree.Contains(192));
    CHECK(!tree.Contains(5));
    const auto* node = tree.Search(64);
    CHECK(node != nullptr && node->key == 64);
}

void TestRemove() {
    Tree tree;
    for (int k : {128, 64, 192, 32}) tree.Insert(static_cast<std::uint8_t>(k));

    CHECK(!tree.Remove(7));           // inexistente
    CHECK(tree.Remove(192));          // folha
    CHECK(!tree.Contains(192));

    CHECK(tree.Remove(64));           // nó interno: 32 assume o lugar
    CHECK(!tree.Contains(64));
    CHECK(tree.Contains(32));
    CHECK(tree.Contains(128));
    CHECK(tree.Size() == 2);

    Tree rootTree;                    // remoção da raiz
    rootTree.Insert(128);
    rootTree.Insert(64);
    CHECK(rootTree.Remove(128));
    CHECK(!rootTree.Contains(128));
    CHECK(rootTree.Contains(64));
    CHECK(rootTree.Size() == 1);
}

void TestClear() {
    Tree tree;
    tree.Insert(1);
    tree.Insert(2);
    tree.Clear();
    CHECK(tree.Empty());
    CHECK(!tree.Contains(1));
}

void TestTraverse() {
    Tree tree;
    for (int k : {128, 64, 192}) tree.Insert(static_cast<std::uint8_t>(k));

    CHECK((Collect(tree, TraversalOrder::PreOrder) == std::vector<int>{128, 64, 192}));
    CHECK((Collect(tree, TraversalOrder::InOrder) == std::vector<int>{64, 128, 192}));
    CHECK((Collect(tree, TraversalOrder::PostOrder) == std::vector<int>{64, 192, 128}));

    bool threw = false;
    try { tree.Traverse(Tree::Visitor{}); } catch (const std::invalid_argument&) { threw = true; }
    CHECK(threw);
}

void TestMove() {
    Tree a;
    a.Insert(10);
    a.Insert(20);

    Tree b(std::move(a));
    CHECK(b.Size() == 2 && b.Contains(10));
    CHECK(a.Empty());

    Tree c;
    c = std::move(b);
    CHECK(c.Size() == 2 && c.Contains(20));
    CHECK(b.Empty());
}

int main() {
    struct { const char* name; void (*fn)(); } tests[] = {
        {"GetBit", TestGetBit},
        {"Arvore vazia", TestEmptyTree},
        {"Insert/Search/Contains", TestInsertAndSearch},
        {"Remove", TestRemove},
        {"Clear", TestClear},
        {"Traverse", TestTraverse},
        {"Move", TestMove},
    };

    for (const auto& t : tests) {
        int before = g_failures;
        t.fn();
        std::cout << (g_failures == before ? "[OK]   " : "[FALHA] ") << t.name << "\n";
    }

    std::cout << (g_failures == 0 ? "\nTodos os testes passaram.\n" : "\nHouve falhas.\n");
    return g_failures == 0 ? 0 : 1;
}
