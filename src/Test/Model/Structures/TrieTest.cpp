// Testes simples para Trie (TRIE.hpp / TRIE.cpp), sem frameworks externos.
//
// Compilar e executar:
//   g++ -std=c++17 -Wall -Wextra TRIE.cpp TRIE_tests.cpp -o trie_tests
//   ./trie_tests
//
// O programa retorna 0 se todos os testes passarem e 1 caso contrário.

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "TRIE.hpp"

namespace {

int g_checks = 0;
int g_failures = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        ++g_checks;                                                          \
        if (!(cond)) {                                                       \
            ++g_failures;                                                    \
            std::cerr << "  FALHOU (linha " << __LINE__ << "): " #cond "\n"; \
        }                                                                    \
    } while (0)

using Words = std::vector<std::string>;
using Ids = std::vector<TaskId>;

bool contains(const Words& words, const std::string& w) {
    for (const auto& x : words) {
        if (x == w) return true;
    }
    return false;
}

// ---------------------------------------------------------------- testes

void testInsertSearchFind() {
    Trie t;
    CHECK(t.insert("casa", 1));
    CHECK(!t.insert("casa", 1));  // associação repetida
    CHECK(t.insert("casa", 2));   // mesma palavra, outra tarefa

    CHECK(t.search("casa"));
    CHECK(!t.search("cas"));      // prefixo não é palavra
    CHECK(!t.search("casas"));
    CHECK(!t.search(""));

    CHECK((t.findTaskIds("casa") == Ids{1, 2}));
    CHECK(t.findTaskIds("outra").empty());
    CHECK(t.findTaskIds("").empty());

    CHECK(t.size() == 1);  // palavras distintas, não associações

    bool threw = false;
    try {
        t.insert("", 3);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    CHECK(threw);
}

void testStartsWith() {
    Trie t;
    CHECK(!t.startsWith(""));  // Trie vazia
    t.insert("casa", 1);

    CHECK(t.startsWith(""));
    CHECK(t.startsWith("ca"));
    CHECK(t.startsWith("casa"));
    CHECK(!t.startsWith("cb"));
    CHECK(!t.startsWith("casas"));
}

void testRemove() {
    Trie t;
    t.insert("casa", 1);
    t.insert("casa", 2);

    CHECK(t.remove("casa", 1));
    CHECK(!t.remove("casa", 1));   // já removida
    CHECK(!t.remove("casa", 99));  // nunca existiu
    CHECK(!t.remove("xyz", 1));    // palavra inexistente
    CHECK(!t.remove("", 1));
    CHECK(t.search("casa"));       // ainda tem a tarefa 2
    CHECK((t.findTaskIds("casa") == Ids{2}));

    CHECK(t.remove("casa", 2));    // última associação
    CHECK(!t.search("casa"));
    CHECK(t.empty());
    CHECK(!t.startsWith("c"));     // ramo podado
}

void testRemovePreservesOtherWords() {
    Trie t;
    t.insert("casa", 1);
    t.insert("casaco", 1);

    CHECK(t.remove("casaco", 1));
    CHECK(t.search("casa"));            // palavra-prefixo preservada
    CHECK(!t.startsWith("casac"));      // só o trecho inútil foi podado

    t.insert("cama", 2);
    CHECK(t.remove("casa", 1));
    CHECK(!t.startsWith("cas"));
    CHECK(t.search("cama"));            // ramo irmão intacto
}

void testRemoveWord() {
    Trie t;
    t.insert("casa", 1);
    t.insert("casa", 2);
    t.insert("cama", 1);

    CHECK(t.removeWord("casa"));
    CHECK(!t.search("casa"));
    CHECK(!t.removeWord("casa"));  // já não existe
    CHECK(!t.removeWord(""));
    CHECK(t.search("cama"));

    // O índice reverso também foi atualizado.
    CHECK((t.wordsOf(1) == Words{"cama"}));
    CHECK(t.wordsOf(2).empty());
}

void testRemoveTask() {
    Trie t;
    t.insert("casa", 1);
    t.insert("bola", 1);
    t.insert("casa", 2);

    CHECK(t.removeTask(1));
    CHECK(!t.removeTask(1));   // já removida
    CHECK(!t.removeTask(99));  // nunca indexada

    CHECK(!t.search("bola"));  // só pertencia à tarefa 1
    CHECK(t.search("casa"));   // ainda pertence à tarefa 2
    CHECK((t.findTaskIds("casa") == Ids{2}));
    CHECK(t.wordsOf(1).empty());
    CHECK(t.size() == 1);
}

void testWordsOf() {
    Trie t;
    t.insert("casa", 1);
    t.insert("bola", 1);

    Words w = t.wordsOf(1);
    CHECK(w.size() == 2);
    CHECK(contains(w, "casa"));
    CHECK(contains(w, "bola"));
    CHECK(t.wordsOf(42).empty());
}

void testUpdateTask() {
    Trie t;
    t.insert("casa", 1);
    t.insert("bola", 1);
    t.insert("casa", 2);

    t.updateTask(1, {"casa", "carro"});  // sai "bola", entra "carro"
    CHECK(!t.search("bola"));
    CHECK(t.search("carro"));
    CHECK((t.findTaskIds("casa") == Ids{1, 2}));  // "casa" preservada

    Words w = t.wordsOf(1);
    CHECK(w.size() == 2);
    CHECK(contains(w, "casa"));
    CHECK(contains(w, "carro"));

    // Palavra vazia: lança exceção e não altera nada.
    bool threw = false;
    try {
        t.updateTask(1, {"nova", ""});
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    CHECK(threw);
    CHECK(!t.search("nova"));
    CHECK(t.search("carro"));

    // Lista vazia equivale a removeTask.
    t.updateTask(1, {});
    CHECK(t.wordsOf(1).empty());
    CHECK(!t.search("carro"));
    CHECK((t.findTaskIds("casa") == Ids{2}));
}

void testAutocomplete() {
    Trie t;
    t.insert("casa", 1);
    t.insert("casaco", 1);
    t.insert("carro", 2);
    t.insert("bola", 3);

    // Ordem lexicográfica; inclui o próprio prefixo se for palavra.
    CHECK((t.autocomplete("ca") == Words{"carro", "casa", "casaco"}));
    CHECK((t.autocomplete("casa") == Words{"casa", "casaco"}));
    CHECK(t.autocomplete("zzz").empty());

    // Prefixo vazio lista tudo.
    CHECK((t.autocomplete("") == Words{"bola", "carro", "casa", "casaco"}));

    // Limite de resultados.
    CHECK((t.autocomplete("ca", 2) == Words{"carro", "casa"}));
    CHECK(t.autocomplete("", 1).size() == 1);
}

void testSizeEmptyClear() {
    Trie t;
    CHECK(t.empty());
    CHECK(t.size() == 0);

    t.insert("casa", 1);
    t.insert("bola", 2);
    CHECK(!t.empty());
    CHECK(t.size() == 2);

    t.clear();
    CHECK(t.empty());
    CHECK(t.size() == 0);
    CHECK(!t.search("casa"));
    CHECK(t.wordsOf(1).empty());  // índice reverso também limpo

    // A Trie continua utilizável após o clear.
    CHECK(t.insert("casa", 1));
    CHECK(t.search("casa"));
}

// ---------------------------------------------------------------- runner

struct TestCase {
    const char* name;
    void (*fn)();
};

}  // namespace

int main() {
    const TestCase tests[] = {
        {"insert/search/findTaskIds", testInsertSearchFind},
        {"startsWith", testStartsWith},
        {"remove", testRemove},
        {"remove preserva outras palavras", testRemovePreservesOtherWords},
        {"removeWord", testRemoveWord},
        {"removeTask", testRemoveTask},
        {"wordsOf", testWordsOf},
        {"updateTask", testUpdateTask},
        {"autocomplete", testAutocomplete},
        {"size/empty/clear", testSizeEmptyClear},
    };

    for (const auto& test : tests) {
        const int before = g_failures;
        std::cout << "[ RUN  ] " << test.name << "\n";
        test.fn();
        std::cout << (g_failures == before ? "[  OK  ] " : "[ FAIL ] ")
                  << test.name << "\n";
    }

    std::cout << "\n" << g_checks << " verificações, " << g_failures
              << " falha(s).\n";
    return g_failures == 0 ? 0 : 1;
}
