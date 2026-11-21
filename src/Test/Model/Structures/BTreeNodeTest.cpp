#include "src/Model/Structures/BTree/BTreeNode.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

static int g_checks = 0;
static int g_failures = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        ++g_checks;                                                          \
        if (!(cond)) {                                                       \
            ++g_failures;                                                    \
            std::cerr << "  FALHA (linha " << __LINE__ << "): " #cond "\n";  \
        }                                                                    \
    } while (0)

#define CHECK_THROWS(expr, ExcType)                                          \
    do {                                                                     \
        ++g_checks;                                                          \
        bool thrown = false;                                                 \
        try { expr; } catch (const ExcType&) { thrown = true; } catch (...) {} \
        if (!thrown) {                                                       \
            ++g_failures;                                                    \
            std::cerr << "  FALHA (linha " << __LINE__ << "): esperava "     \
                      << #ExcType << " em: " #expr "\n";                     \
        }                                                                    \
    } while (0)

#define RUN(test)                                                            \
    do {                                                                     \
        std::cout << "[ RUN ] " #test "\n";                                  \
        test();                                                              \
    } while (0)

// ---------------------------------------------------------------------------

void testConstrutor() {
    CHECK_THROWS(BTreeNode(1, true), std::invalid_argument);

    BTreeNode node(3, true);
    CHECK(node.isLeaf());
    CHECK(node.isEmpty());
    CHECK(!node.isFull());
    CHECK(node.keyCount() == 0);
    CHECK(node.childCount() == 0);
    CHECK(node.minDegree() == 3);
    CHECK(node.maxKeys() == 5);      // 2t - 1
    CHECK(node.maxChildren() == 6);  // 2t

    BTreeNode internal(2, false);
    CHECK(!internal.isLeaf());
}

void testInsertKey() {
    BTreeNode node(2, true);  // máximo de 3 chaves

    CHECK(node.insertKey(20) == 0);
    CHECK(node.insertKey(10) == 0);
    CHECK(node.insertKey(30) == 2);
    CHECK(node.keyCount() == 3);
    CHECK(node.isFull());
    CHECK(!node.isEmpty());

    // Chaves ficam ordenadas
    CHECK(node.getKey(0) == 10);
    CHECK(node.getKey(1) == 20);
    CHECK(node.getKey(2) == 30);

    CHECK_THROWS(node.insertKey(40), std::length_error);  // nó cheio

    BTreeNode other(2, true);
    other.insertKey(5);
    CHECK_THROWS(other.insertKey(5), std::invalid_argument);  // duplicada
}

void testBusca() {
    BTreeNode node(3, true);
    node.insertKey(10);
    node.insertKey(20);
    node.insertKey(30);

    CHECK(node.findKey(10).has_value() && *node.findKey(10) == 0);
    CHECK(node.findKey(30).has_value() && *node.findKey(30) == 2);
    CHECK(!node.findKey(25).has_value());

    CHECK(node.lowerBound(5) == 0);
    CHECK(node.lowerBound(20) == 1);   // chave existente
    CHECK(node.lowerBound(25) == 2);   // entre chaves
    CHECK(node.lowerBound(99) == 3);   // maior que todas -> keyCount()
}

void testRemoveKey() {
    BTreeNode node(2, true);
    node.insertKey(10);
    node.insertKey(20);
    node.insertKey(30);

    CHECK(node.removeKey(1) == 20);
    CHECK(node.keyCount() == 2);
    CHECK(node.getKey(0) == 10);
    CHECK(node.getKey(1) == 30);
    CHECK(!node.findKey(20).has_value());

    CHECK_THROWS(node.removeKey(2), std::out_of_range);
}

void testGetKeyInvalido() {
    BTreeNode node(2, true);
    node.insertKey(1);
    CHECK(node.getKey(0) == 1);
    CHECK_THROWS(node.getKey(1), std::out_of_range);
}

void testFilhos() {
    BTreeNode parent(2, false);  // máximo de 4 filhos
    BTreeNode a(2, true), b(2, true), c(2, true), d(2, true), e(2, true);

    parent.insertChild(0, &a);
    parent.insertChild(1, &c);
    parent.insertChild(1, &b);  // insere no meio
    CHECK(parent.childCount() == 3);
    CHECK(parent.getChild(0) == &a);
    CHECK(parent.getChild(1) == &b);
    CHECK(parent.getChild(2) == &c);

    const BTreeNode& constParent = parent;
    CHECK(constParent.getChild(0) == &a);

    parent.insertChild(3, &d);
    CHECK_THROWS(parent.insertChild(4, &e), std::length_error);  // máximo de filhos

    CHECK_THROWS(parent.insertChild(0, nullptr), std::invalid_argument);
    CHECK_THROWS(parent.getChild(4), std::out_of_range);

    CHECK(parent.removeChild(1) == &b);  // devolve o ponteiro, sem destruir
    CHECK(parent.childCount() == 3);
    CHECK(parent.getChild(1) == &c);
    CHECK_THROWS(parent.removeChild(3), std::out_of_range);

    BTreeNode leaf(2, true);
    CHECK_THROWS(leaf.insertChild(0, &a), std::logic_error);  // folha

    BTreeNode small(2, false);
    CHECK_THROWS(small.insertChild(1, &a), std::out_of_range);  // index > childCount()
}

void testClear() {
    BTreeNode parent(2, false);
    BTreeNode child(2, true);
    parent.insertKey(10);
    parent.insertChild(0, &child);

    parent.clear();
    CHECK(parent.isEmpty());
    CHECK(parent.childCount() == 0);
}

void testValidate() {
    std::string error;

    // Folha válida
    BTreeNode leaf(2, true);
    leaf.insertKey(1);
    leaf.insertKey(2);
    CHECK(leaf.validate());
    CHECK(leaf.validate(&error));

    // Nó interno válido: n chaves e n + 1 filhos
    BTreeNode a(2, true), b(2, true);
    BTreeNode internal(2, false);
    internal.insertKey(10);
    internal.insertChild(0, &a);
    internal.insertChild(1, &b);
    CHECK(internal.validate(&error));

    // Nó interno vazio (sem chaves e sem filhos) é aceito
    BTreeNode empty(2, false);
    CHECK(empty.validate());

    // Nó interno com número de filhos incorreto
    BTreeNode bad(2, false);
    bad.insertKey(10);
    bad.insertChild(0, &a);
    error.clear();
    CHECK(!bad.validate(&error));
    CHECK(!error.empty());
    CHECK(!bad.validate());  // error == nullptr não deve quebrar
}

// ---------------------------------------------------------------------------

int main() {
    RUN(testConstrutor);
    RUN(testInsertKey);
    RUN(testBusca);
    RUN(testRemoveKey);
    RUN(testGetKeyInvalido);
    RUN(testFilhos);
    RUN(testClear);
    RUN(testValidate);

    std::cout << "\n" << (g_checks - g_failures) << "/" << g_checks
              << " verificacoes passaram.\n";
    return g_failures == 0 ? 0 : 1;
}
