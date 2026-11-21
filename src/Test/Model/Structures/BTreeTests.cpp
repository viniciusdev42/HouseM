#include <cstddef>
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <stdexcept>
#include <utility>
#include <vector>

#include "src/Main/Model/Structures/BTree/BTree.hpp"
#include "src/Main/Model/Structures/BTree/BTreeNode.hpp"

// ===========================================================================
// Mini framework de testes
// ===========================================================================

namespace {

int g_checks = 0;
int g_failures = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        ++g_checks;                                                          \
        if (!(cond)) {                                                       \
            ++g_failures;                                                    \
            std::cerr << "  FALHOU: " #cond " (" << __FILE__ << ":"          \
                      << __LINE__ << ")\n";                                  \
        }                                                                    \
    } while (0)

#define RUN(test)                                                            \
    do {                                                                     \
        const int before = g_failures;                                       \
        test();                                                              \
        std::cout << (g_failures == before ? "[OK]    " : "[FALHA] ")        \
                  << #test << "\n";                                          \
    } while (0)

using Key = BTree::Key;

Key K(int value) { return static_cast<Key>(value); }

// true se `fn` lança exatamente uma exceção do tipo Ex (ou derivada).
template <typename Ex, typename Fn>
bool throwsAs(Fn fn) {
    try {
        fn();
    } catch (const Ex&) {
        return true;
    } catch (...) {
        return false;
    }
    return false;
}

// Devolve todas as chaves em ordem, via traverseInOrder.
std::vector<Key> collect(const BTree& tree) {
    std::vector<Key> keys;
    tree.traverseInOrder([&](const Key& k) { keys.push_back(k); });
    return keys;
}

// Árvore com as chaves 1..n inseridas em ordem crescente.
BTree makeSequential(int n, std::size_t minDegree = 2) {
    BTree tree(minDegree);
    for (int i = 1; i <= n; ++i) {
        tree.insert(K(i));
    }
    return tree;
}

// ===========================================================================
// Ciclo de vida e estado
// ===========================================================================

void testConstructor() {
    bool threw = false;
    try {
        BTree invalid(1);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    CHECK(threw);

    BTree tree(3);
    CHECK(tree.minDegree() == 3);
    CHECK(tree.empty());
    CHECK(tree.size() == 0);
    CHECK(tree.height() == 0);
}

void testEmptyTreeQueries() {
    BTree tree;
    CHECK(!tree.contains(K(1)));
    CHECK(!tree.findMin().has_value());
    CHECK(!tree.findMax().has_value());
    CHECK(!tree.lowerBound(K(1)).has_value());
    CHECK(!tree.upperBound(K(1)).has_value());
    CHECK(tree.rangeSearch(K(0), K(10)).empty());
    CHECK(collect(tree).empty());
    CHECK(!tree.remove(K(1)));
}

void testMoveAndClear() {
    BTree a = makeSequential(20);
    BTree b(std::move(a));
    CHECK(a.empty());
    CHECK(b.size() == 20);
    CHECK(b.contains(K(10)));

    BTree c;
    c = std::move(b);
    CHECK(b.empty());
    CHECK(c.size() == 20);

    c.clear();
    CHECK(c.empty());
    CHECK(c.height() == 0);
    CHECK(!c.contains(K(10)));

    c.insert(K(5));  // continua utilizável após clear()
    CHECK(c.size() == 1);
}

// ===========================================================================
// Inserção e busca
// ===========================================================================

void testInsertAndContains() {
    BTree tree;
    CHECK(tree.insert(K(10)));
    CHECK(tree.insert(K(20)));
    CHECK(tree.insert(K(5)));

    CHECK(tree.size() == 3);
    CHECK(!tree.empty());
    CHECK(tree.height() == 1);
    CHECK(tree.contains(K(10)));
    CHECK(tree.contains(K(5)));
    CHECK(!tree.contains(K(15)));
}

void testInsertDuplicate() {
    BTree tree = makeSequential(10);
    CHECK(!tree.insert(K(5)));
    CHECK(tree.size() == 10);
    CHECK(collect(tree).size() == 10);
}

void testInsertGrowsHeight() {
    BTree tree(2);  // nó cheio com 3 chaves
    for (int i = 1; i <= 3; ++i) tree.insert(K(i));
    CHECK(tree.height() == 1);

    tree.insert(K(4));  // força divisão da raiz
    CHECK(tree.height() == 2);

    for (int i = 5; i <= 100; ++i) tree.insert(K(i));
    CHECK(tree.height() > 2);
    CHECK(tree.size() == 100);
    for (int i = 1; i <= 100; ++i) CHECK(tree.contains(K(i)));
}

// ===========================================================================
// Consultas ordenadas
// ===========================================================================

void testMinMax() {
    BTree tree;
    for (int v : {50, 10, 90, 30, 70, 20, 80}) tree.insert(K(v));
    CHECK(tree.findMin().value() == K(10));
    CHECK(tree.findMax().value() == K(90));
}

void testLowerUpperBound() {
    BTree tree;
    for (int v : {10, 20, 30, 40, 50, 60, 70, 80}) tree.insert(K(v));

    // lowerBound: menor chave >= x
    CHECK(tree.lowerBound(K(30)).value() == K(30));
    CHECK(tree.lowerBound(K(35)).value() == K(40));
    CHECK(tree.lowerBound(K(1)).value() == K(10));
    CHECK(!tree.lowerBound(K(81)).has_value());

    // upperBound: menor chave > x
    CHECK(tree.upperBound(K(30)).value() == K(40));
    CHECK(tree.upperBound(K(35)).value() == K(40));
    CHECK(tree.upperBound(K(1)).value() == K(10));
    CHECK(!tree.upperBound(K(80)).has_value());
}

void testRangeSearch() {
    BTree tree = makeSequential(50);

    std::vector<Key> expected;
    for (int i = 10; i <= 20; ++i) expected.push_back(K(i));
    CHECK(tree.rangeSearch(K(10), K(20)) == expected);

    CHECK(tree.rangeSearch(K(7), K(7)) == std::vector<Key>{K(7)});
    CHECK(tree.rangeSearch(K(20), K(10)).empty());   // low > high
    CHECK(tree.rangeSearch(K(100), K(200)).empty()); // fora do intervalo
    CHECK(tree.rangeSearch(K(-10), K(1000)).size() == 50);
}

void testTraverseInOrder() {
    BTree tree;
    for (int v : {42, 7, 99, 1, 63, 28, 15, 80, 3, 55}) tree.insert(K(v));

    const std::vector<Key> keys = collect(tree);
    CHECK(keys.size() == 10);
    bool sorted = true;
    for (std::size_t i = 1; i < keys.size(); ++i) {
        if (!(keys[i - 1] < keys[i])) sorted = false;
    }
    CHECK(sorted);

    tree.traverseInOrder(nullptr);  // função vazia não deve quebrar
}

// ===========================================================================
// Remoção
// ===========================================================================

void testRemoveBasic() {
    BTree tree;
    for (int v : {10, 20, 30}) tree.insert(K(v));

    CHECK(!tree.remove(K(99)));  // inexistente
    CHECK(tree.size() == 3);

    CHECK(tree.remove(K(20)));
    CHECK(tree.size() == 2);
    CHECK(!tree.contains(K(20)));
    CHECK(tree.contains(K(10)));
    CHECK(!tree.remove(K(20)));  // já removida
}

void testRemoveUntilEmpty() {
    BTree tree = makeSequential(30);
    for (int i = 1; i <= 30; ++i) CHECK(tree.remove(K(i)));

    CHECK(tree.empty());
    CHECK(tree.size() == 0);
    CHECK(tree.height() == 0);

    tree.insert(K(1));  // reutilizável após esvaziar
    CHECK(tree.contains(K(1)));
}

void testRemoveShrinksHeight() {
    BTree tree = makeSequential(100);
    const std::size_t initial = tree.height();
    CHECK(initial > 2);

    for (int i = 1; i <= 95; ++i) tree.remove(K(i));
    CHECK(tree.height() < initial);
    CHECK(tree.size() == 5);
}

// Remove em ordens diferentes (cobre empréstimo, fusão e chaves internas).
void testRemoveOrders() {
    for (std::size_t t : {2, 3}) {
        // Ordem decrescente
        BTree down = makeSequential(60, t);
        for (int i = 60; i >= 1; --i) CHECK(down.remove(K(i)));
        CHECK(down.empty());

        // Remove primeiro os pares, depois os ímpares
        BTree mixed = makeSequential(60, t);
        for (int i = 2; i <= 60; i += 2) CHECK(mixed.remove(K(i)));
        CHECK(mixed.size() == 30);
        for (int i = 1; i <= 60; ++i) {
            CHECK(mixed.contains(K(i)) == (i % 2 == 1));
        }
        for (int i = 1; i <= 60; i += 2) CHECK(mixed.remove(K(i)));
        CHECK(mixed.empty());
    }
}

// ===========================================================================
// BTreeNode
// ===========================================================================

void testNodeConstructor() {
    CHECK(throwsAs<std::invalid_argument>([] { BTreeNode n(1, true); }));

    BTreeNode leaf(3, true);
    CHECK(leaf.isLeaf());
    CHECK(leaf.isEmpty());
    CHECK(!leaf.isFull());
    CHECK(leaf.keyCount() == 0);
    CHECK(leaf.childCount() == 0);
    CHECK(leaf.minDegree() == 3);
    CHECK(leaf.maxKeys() == 5);      // 2t - 1
    CHECK(leaf.maxChildren() == 6);  // 2t

    BTreeNode internal(2, false);
    CHECK(!internal.isLeaf());
}

void testNodeInsertKey() {
    BTreeNode node(2, true);  // até 3 chaves

    CHECK(node.insertKey(K(20)) == 0);
    CHECK(node.insertKey(K(10)) == 0);  // entra antes
    CHECK(node.insertKey(K(30)) == 2);  // entra no fim
    CHECK(node.keyCount() == 3);
    CHECK(node.isFull());
    CHECK(!node.isEmpty());

    CHECK(node.getKey(0) == K(10));
    CHECK(node.getKey(1) == K(20));
    CHECK(node.getKey(2) == K(30));

    CHECK(throwsAs<std::length_error>([&] { node.insertKey(K(40)); }));

    node.removeKey(2);
    CHECK(throwsAs<std::invalid_argument>([&] { node.insertKey(K(10)); }));
}

void testNodeSearch() {
    BTreeNode node(3, true);
    for (int v : {10, 20, 30}) node.insertKey(K(v));

    CHECK(node.findKey(K(20)).value() == 1);
    CHECK(!node.findKey(K(25)).has_value());

    CHECK(node.lowerBound(K(5)) == 0);
    CHECK(node.lowerBound(K(20)) == 1);
    CHECK(node.lowerBound(K(25)) == 2);
    CHECK(node.lowerBound(K(99)) == 3);  // == keyCount()
}

void testNodeRemoveKey() {
    BTreeNode node(2, true);
    for (int v : {10, 20, 30}) node.insertKey(K(v));

    CHECK(node.removeKey(1) == K(20));
    CHECK(node.keyCount() == 2);
    CHECK(node.getKey(1) == K(30));

    CHECK(throwsAs<std::out_of_range>([&] { node.removeKey(2); }));
    CHECK(throwsAs<std::out_of_range>([&] { node.getKey(2); }));
}

void testNodeChildren() {
    BTreeNode parent(2, false);  // até 4 filhos
    std::vector<std::unique_ptr<BTreeNode>> owned;  // o nó não é dono
    for (int i = 0; i < 5; ++i) {
        owned.push_back(std::make_unique<BTreeNode>(2, true));
    }

    parent.insertChild(0, owned[0].get());
    parent.insertChild(1, owned[1].get());
    parent.insertChild(1, owned[2].get());  // insere no meio
    CHECK(parent.childCount() == 3);
    CHECK(parent.getChild(0) == owned[0].get());
    CHECK(parent.getChild(1) == owned[2].get());
    CHECK(parent.getChild(2) == owned[1].get());

    parent.insertChild(3, owned[3].get());
    CHECK(throwsAs<std::length_error>(
        [&] { parent.insertChild(0, owned[4].get()); }));  // máximo = 4

    // removeChild devolve o ponteiro sem destruir o filho
    CHECK(parent.removeChild(1) == owned[2].get());
    CHECK(parent.childCount() == 3);
    CHECK(throwsAs<std::out_of_range>([&] { parent.removeChild(3); }));
    CHECK(throwsAs<std::out_of_range>([&] { parent.getChild(3); }));

    // clear esquece chaves e filhos, sem deletá-los
    parent.insertKey(K(1));
    parent.clear();
    CHECK(parent.keyCount() == 0);
    CHECK(parent.childCount() == 0);
}

void testNodeChildErrors() {
    BTreeNode leaf(2, true);
    BTreeNode child(2, true);
    CHECK(throwsAs<std::logic_error>([&] { leaf.insertChild(0, &child); }));

    BTreeNode internal(2, false);
    CHECK(throwsAs<std::invalid_argument>(
        [&] { internal.insertChild(0, nullptr); }));
    CHECK(throwsAs<std::out_of_range>(
        [&] { internal.insertChild(1, &child); }));  // index > childCount()
}

void testNodeValidate() {
    std::string error;

    BTreeNode leaf(2, true);
    leaf.insertKey(K(1));
    CHECK(leaf.validate());
    CHECK(leaf.validate(&error));

    BTreeNode emptyInternal(2, false);  // sem chaves e sem filhos é aceito
    CHECK(emptyInternal.validate());

    BTreeNode a(2, true), b(2, true);
    BTreeNode internal(2, false);
    internal.insertKey(K(10));
    internal.insertChild(0, &a);
    CHECK(!internal.validate(&error));  // 1 chave exige 2 filhos
    CHECK(!error.empty());

    internal.insertChild(1, &b);
    CHECK(internal.validate());
}

// ===========================================================================
// Comparação com std::set (inserções e remoções intercaladas)
// ===========================================================================

void testAgainstStdSet() {
    for (std::size_t t : {2, 3, 5}) {
        BTree tree(t);
        std::set<int> reference;

        // Sequência pseudoaleatória determinística (sem <random>).
        unsigned state = 12345;
        auto next = [&state]() {
            state = state * 1103515245u + 12345u;
            return static_cast<int>((state >> 16) % 200);
        };

        bool consistent = true;
        for (int step = 0; step < 1000; ++step) {
            const int value = next();
            if (step % 3 == 2) {
                if (tree.remove(K(value)) != (reference.erase(value) > 0)) {
                    consistent = false;
                }
            } else {
                if (tree.insert(K(value)) != reference.insert(value).second) {
                    consistent = false;
                }
            }
            if (tree.size() != reference.size()) consistent = false;
        }
        CHECK(consistent);

        std::vector<Key> expected;
        for (int v : reference) expected.push_back(K(v));
        CHECK(collect(tree) == expected);
    }
}

}  // namespace

// ===========================================================================
// main
// ===========================================================================

int main() {
    RUN(testConstructor);
    RUN(testEmptyTreeQueries);
    RUN(testMoveAndClear);

    RUN(testInsertAndContains);
    RUN(testInsertDuplicate);
    RUN(testInsertGrowsHeight);

    RUN(testMinMax);
    RUN(testLowerUpperBound);
    RUN(testRangeSearch);
    RUN(testTraverseInOrder);

    RUN(testRemoveBasic);
    RUN(testRemoveUntilEmpty);
    RUN(testRemoveShrinksHeight);
    RUN(testRemoveOrders);

    RUN(testAgainstStdSet);

    RUN(testNodeConstructor);
    RUN(testNodeInsertKey);
    RUN(testNodeSearch);
    RUN(testNodeRemoveKey);
    RUN(testNodeChildren);
    RUN(testNodeChildErrors);
    RUN(testNodeValidate);

    std::cout << "\n" << (g_checks - g_failures) << "/" << g_checks
              << " verificacoes passaram.\n";
    return g_failures == 0 ? 0 : 1;
}
