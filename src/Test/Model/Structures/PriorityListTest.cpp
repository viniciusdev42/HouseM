#include "src/Model/Structures/PriorityList/PriorityList.hpp"

#include <iostream>
#include <stdexcept>
#include <vector>

// ---------------------------------------------------------------------------
// Mini-framework de testes
// ---------------------------------------------------------------------------

static int failures = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            ++failures;                                                      \
            std::cerr << "  FALHOU (linha " << __LINE__ << "): " #cond "\n"; \
        }                                                                    \
    } while (false)

#define CHECK_THROWS(expr, ExceptionType)                                    \
    do {                                                                     \
        bool thrown = false;                                                 \
        try {                                                                \
            (void)(expr);                                                    \
        } catch (const ExceptionType&) {                                     \
            thrown = true;                                                   \
        } catch (...) {                                                      \
        }                                                                    \
        if (!thrown) {                                                       \
            ++failures;                                                      \
            std::cerr << "  FALHOU (linha " << __LINE__                      \
                      << "): esperava " #ExceptionType " em " #expr "\n";    \
        }                                                                    \
    } while (false)

#define RUN(test)                                                            \
    do {                                                                     \
        std::cout << "[ RUN ] " #test "\n";                                  \
        test();                                                              \
    } while (false)

// ---------------------------------------------------------------------------
// Testes
// ---------------------------------------------------------------------------

void testListaVazia() {
    PriorityList list;
    CHECK(list.empty());
    CHECK(list.size() == 0);
    CHECK(!list.contains(1));
    CHECK_THROWS(list.peek(), std::out_of_range);
    CHECK_THROWS(list.extractTop(), std::out_of_range);
}

void testInsertEPeek() {
    PriorityList list;
    list.insert({1, "baixa", 10});
    list.insert({2, "alta", 50});
    list.insert({3, "media", 30});

    CHECK(list.size() == 3);
    CHECK(!list.empty());
    CHECK(list.contains(2));
    CHECK(list.peek().id == 2);  // maior prioridade no topo
    CHECK(list.size() == 3);     // peek não remove
}

void testInsertIdDuplicado() {
    PriorityList list;
    list.insert({1, "a", 10});
    CHECK_THROWS(list.insert({1, "b", 99}), std::invalid_argument);
    CHECK(list.size() == 1);
}

void testExtractTopEmOrdemDecrescente() {
    PriorityList list;
    list.insert({1, "a", 20});
    list.insert({2, "b", 5});
    list.insert({3, "c", 40});
    list.insert({4, "d", 30});

    CHECK(list.extractTop().priority == 40);
    CHECK(list.extractTop().priority == 30);
    CHECK(list.extractTop().priority == 20);
    CHECK(list.extractTop().priority == 5);
    CHECK(list.empty());
}

void testConstrutorComColecao() {
    PriorityList list(std::vector<Element>{
        {1, "a", 3}, {2, "b", 9}, {3, "c", 1}, {4, "d", 7}, {5, "e", 5}});

    CHECK(list.size() == 5);
    CHECK(list.extractTop().id == 2);
    CHECK(list.extractTop().id == 4);
    CHECK(list.extractTop().id == 5);
    CHECK(list.extractTop().id == 1);
    CHECK(list.extractTop().id == 3);

    CHECK_THROWS(PriorityList(std::vector<Element>{{1, "a", 1}, {1, "b", 2}}),
                 std::invalid_argument);
}

void testUpdatePriority() {
    PriorityList list;
    list.insert({1, "a", 10});
    list.insert({2, "b", 20});
    list.insert({3, "c", 30});

    list.updatePriority(1, 100);  // sobe até o topo
    CHECK(list.peek().id == 1);

    list.updatePriority(1, 0);  // desce até o fim
    CHECK(list.peek().id == 3);
    CHECK(list.extractTop().id == 3);
    CHECK(list.extractTop().id == 2);
    CHECK(list.extractTop().id == 1);

    CHECK_THROWS(list.updatePriority(99, 1), std::invalid_argument);
}

void testRemove() {
    PriorityList list;
    list.insert({1, "a", 10});
    list.insert({2, "b", 20});
    list.insert({3, "c", 30});
    list.insert({4, "d", 40});

    const Element removed = list.remove(3);  // nó do meio
    CHECK(removed.id == 3);
    CHECK(removed.name == "c");
    CHECK(!list.contains(3));
    CHECK(list.size() == 3);

    CHECK(list.extractTop().id == 4);
    CHECK(list.extractTop().id == 2);
    CHECK(list.extractTop().id == 1);

    CHECK_THROWS(list.remove(3), std::invalid_argument);
}

// ---------------------------------------------------------------------------

int main() {
    RUN(testListaVazia);
    RUN(testInsertEPeek);
    RUN(testInsertIdDuplicado);
    RUN(testExtractTopEmOrdemDecrescente);
    RUN(testConstrutorComColecao);
    RUN(testUpdatePriority);
    RUN(testRemove);

    if (failures == 0) {
        std::cout << "\nTodos os testes passaram.\n";
        return 0;
    }
    std::cout << "\n" << failures << " verificacao(oes) falharam.\n";
    return 1;
}
