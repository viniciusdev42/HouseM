#include <iostream>
#include <stdexcept>
#include <string>

#include "src/Main/Model/Structures/HashTable/HashTable.hpp"

// ---------------------------------------------------------------------------
// Mini framework de testes
// ---------------------------------------------------------------------------

static int g_failures = 0;
static int g_checks = 0;

#define CHECK(cond)                                                         \
    do {                                                                    \
        ++g_checks;                                                         \
        if (!(cond)) {                                                      \
            ++g_failures;                                                   \
            std::cerr << "    FALHOU (linha " << __LINE__ << "): " #cond    \
                      << "\n";                                              \
        }                                                                   \
    } while (false)

#define CHECK_THROWS(expr, ExceptionType)                                   \
    do {                                                                    \
        ++g_checks;                                                         \
        bool thrown = false;                                                \
        try {                                                               \
            expr;                                                           \
        } catch (const ExceptionType&) {                                    \
            thrown = true;                                                  \
        } catch (...) {                                                     \
        }                                                                   \
        if (!thrown) {                                                      \
            ++g_failures;                                                   \
            std::cerr << "    FALHOU (linha " << __LINE__                   \
                      << "): esperava exceção em " #expr "\n";              \
        }                                                                   \
    } while (false)

#define RUN(test)                                                           \
    do {                                                                    \
        int before = g_failures;                                            \
        test();                                                             \
        std::cout << (g_failures == before ? "[OK]    " : "[FALHA] ")       \
                  << #test << "\n";                                         \
    } while (false)

// ---------------------------------------------------------------------------
// Testes
// ---------------------------------------------------------------------------

void test_construtor() {
    HashTable t;
    CHECK(t.empty());
    CHECK(t.size() == 0);
    CHECK(t.bucketCount() == HashTable::kDefaultInitialCapacity);
    CHECK(t.maxLoadFactor() == HashTable::kDefaultMaxLoadFactor);
    CHECK(t.loadFactor() == 0.0);

    HashTable t2(16);
    CHECK(t2.bucketCount() == 16);

    CHECK_THROWS(HashTable(0), std::invalid_argument);
}

void test_insert_e_search() {
    HashTable t;
    CHECK(t.insert("a", 1));
    CHECK(t.insert("b", 2));
    CHECK(t.size() == 2);
    CHECK(!t.empty());

    auto a = t.search("a");
    CHECK(a.has_value() && *a == 1);
    auto b = t.search("b");
    CHECK(b.has_value() && *b == 2);

    CHECK(!t.search("inexistente").has_value());
}

void test_insert_atualiza_valor() {
    HashTable t;
    CHECK(t.insert("x", 10));
    CHECK(!t.insert("x", 20));  // atualização retorna false
    CHECK(t.size() == 1);
    CHECK(*t.search("x") == 20);
}

void test_contains() {
    HashTable t;
    t.insert("k", 5);
    CHECK(t.contains("k"));
    CHECK(!t.contains("outra"));
}

void test_remove() {
    HashTable t;
    t.insert("a", 1);
    t.insert("b", 2);

    CHECK(t.remove("a"));
    CHECK(!t.contains("a"));
    CHECK(t.contains("b"));
    CHECK(t.size() == 1);

    CHECK(!t.remove("a"));  // já removida
    CHECK(!t.remove("nunca_existiu"));
    CHECK(t.size() == 1);
}

void test_colisoes() {
    // Capacidade 1 e fator de carga alto: todas as chaves caem no mesmo
    // bucket, forçando o uso das cadeias (remoção de cabeça, meio e fim).
    HashTable t(1);
    t.setMaxLoadFactor(100.0);
    t.insert("a", 1);
    t.insert("b", 2);
    t.insert("c", 3);
    CHECK(t.bucketCount() == 1);

    CHECK(*t.search("a") == 1);
    CHECK(*t.search("b") == 2);
    CHECK(*t.search("c") == 3);

    CHECK(t.remove("b"));  // meio/fim da cadeia
    CHECK(t.remove("c"));  // cabeça da cadeia (inserido por último)
    CHECK(t.remove("a"));
    CHECK(t.empty());
}

void test_clear() {
    HashTable t;
    for (int i = 0; i < 50; ++i) t.insert("k" + std::to_string(i), i);
    const std::size_t buckets = t.bucketCount();

    t.clear();
    CHECK(t.empty());
    CHECK(t.size() == 0);
    CHECK(t.bucketCount() == buckets);  // mantém a capacidade
    CHECK(!t.contains("k0"));

    CHECK(t.insert("novo", 1));  // continua utilizável
}

void test_crescimento_automatico() {
    HashTable t;
    const int n = 1000;
    for (int i = 0; i < n; ++i) t.insert("chave" + std::to_string(i), i);

    CHECK(t.size() == static_cast<std::size_t>(n));
    CHECK(t.bucketCount() > HashTable::kDefaultInitialCapacity);
    CHECK(t.loadFactor() <= t.maxLoadFactor());

    bool todosEncontrados = true;
    for (int i = 0; i < n; ++i) {
        auto v = t.search("chave" + std::to_string(i));
        if (!v.has_value() || *v != i) todosEncontrados = false;
    }
    CHECK(todosEncontrados);
}

void test_reducao_automatica() {
    HashTable t;
    for (int i = 0; i < 100; ++i) t.insert("k" + std::to_string(i), i);
    const std::size_t grown = t.bucketCount();
    CHECK(grown > HashTable::kDefaultInitialCapacity);

    for (int i = 0; i < 100; ++i) t.remove("k" + std::to_string(i));
    CHECK(t.empty());
    CHECK(t.bucketCount() < grown);
    CHECK(t.bucketCount() >= HashTable::kDefaultInitialCapacity);
}

void test_rehash() {
    HashTable t;
    for (int i = 0; i < 6; ++i) t.insert("k" + std::to_string(i), i);

    t.rehash(64);
    CHECK(t.bucketCount() == 64);
    CHECK(t.size() == 6);
    bool ok = true;
    for (int i = 0; i < 6; ++i) {
        auto v = t.search("k" + std::to_string(i));
        if (!v.has_value() || *v != i) ok = false;
    }
    CHECK(ok);

    // Pedido pequeno demais é elevado ao mínimo exigido pelo fator de carga.
    t.rehash(1);
    CHECK(t.loadFactor() <= t.maxLoadFactor());
    CHECK(t.size() == 6);
    CHECK(*t.search("k3") == 3);
}

void test_max_load_factor() {
    HashTable t;
    t.setMaxLoadFactor(0.5);
    CHECK(t.maxLoadFactor() == 0.5);

    for (int i = 0; i < 20; ++i) t.insert("k" + std::to_string(i), i);
    CHECK(t.loadFactor() <= 0.5);

    CHECK_THROWS(t.setMaxLoadFactor(0.0), std::invalid_argument);
    CHECK_THROWS(t.setMaxLoadFactor(-1.0), std::invalid_argument);
    CHECK(t.maxLoadFactor() == 0.5);  // valor anterior preservado
}

void test_copia() {
    HashTable a;
    a.insert("x", 1);
    a.insert("y", 2);

    HashTable b(a);  // construtor de cópia
    CHECK(b.size() == 2);
    CHECK(*b.search("x") == 1);

    b.insert("x", 99);  // cópia é independente
    b.insert("z", 3);
    CHECK(*a.search("x") == 1);
    CHECK(!a.contains("z"));

    HashTable c;
    c.insert("lixo", 0);
    c = a;  // atribuição por cópia
    CHECK(c.size() == 2);
    CHECK(!c.contains("lixo"));
    CHECK(*c.search("y") == 2);

    c = c;  // auto-atribuição
    CHECK(c.size() == 2);
}

void test_move() {
    HashTable a;
    a.insert("x", 1);
    a.insert("y", 2);

    HashTable b(std::move(a));  // construtor de movimentação
    CHECK(b.size() == 2);
    CHECK(*b.search("x") == 1);

    // Objeto movido fica vazio, porém válido e reutilizável.
    CHECK(a.empty());
    CHECK(!a.contains("x"));
    CHECK(a.insert("novo", 7));
    CHECK(*a.search("novo") == 7);

    HashTable c;
    c = std::move(b);  // atribuição por movimentação
    CHECK(c.size() == 2);
    CHECK(*c.search("y") == 2);
}

void test_swap() {
    HashTable a, b;
    a.insert("a", 1);
    b.insert("b", 2);
    b.insert("c", 3);

    a.swap(b);
    CHECK(a.size() == 2);
    CHECK(b.size() == 1);
    CHECK(a.contains("b") && a.contains("c"));
    CHECK(b.contains("a"));
}

// ---------------------------------------------------------------------------

int main() {
    RUN(test_construtor);
    RUN(test_insert_e_search);
    RUN(test_insert_atualiza_valor);
    RUN(test_contains);
    RUN(test_remove);
    RUN(test_colisoes);
    RUN(test_clear);
    RUN(test_crescimento_automatico);
    RUN(test_reducao_automatica);
    RUN(test_rehash);
    RUN(test_max_load_factor);
    RUN(test_copia);
    RUN(test_move);
    RUN(test_swap);

    std::cout << "\n" << (g_checks - g_failures) << "/" << g_checks
              << " verificações passaram.\n";
    return g_failures == 0 ? 0 : 1;
}
