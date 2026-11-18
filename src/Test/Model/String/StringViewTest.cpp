// Testes simples para StringView (sem frameworks externos).
//
// Compilar (exemplo):
//   g++ -std=c++17 -Wall -Wextra StringView_test.cpp StringView.cpp -o stringview_test
// Executar:
//   ./stringview_test
//
// O programa retorna 0 se todos os testes passarem e 1 caso contrário.

#include "StringView.hpp"

#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string>

using SV = StringView::StringView;

namespace {

int g_failures = 0;
int g_checks = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        ++g_checks;                                                        \
        if (!(cond)) {                                                     \
            ++g_failures;                                                  \
            std::cerr << __FILE__ << ":" << __LINE__                       \
                      << ": CHECK falhou: " #cond "\n";                    \
        }                                                                  \
    } while (false)

#define CHECK_THROWS_OUT_OF_RANGE(expr)                                    \
    do {                                                                   \
        ++g_checks;                                                        \
        bool threw = false;                                                \
        try {                                                              \
            (void)(expr);                                                  \
        } catch (const std::out_of_range&) {                               \
            threw = true;                                                  \
        } catch (...) {                                                    \
        }                                                                  \
        if (!threw) {                                                      \
            ++g_failures;                                                  \
            std::cerr << __FILE__ << ":" << __LINE__                       \
                      << ": esperado std::out_of_range: " #expr "\n";      \
        }                                                                  \
    } while (false)

// ----------------------------------------------------------------------
// Construção
// ----------------------------------------------------------------------

void testConstruction() {
    SV empty;
    CHECK(empty.empty());
    CHECK(empty.size() == 0);
    CHECK(empty.data() == nullptr);

    const char raw[] = "abcdef";
    SV fromPointerAndSize(raw, 3);
    CHECK(fromPointerAndSize.size() == 3);
    CHECK(fromPointerAndSize.data() == raw);

    SV fromCString("hello");
    CHECK(fromCString.size() == 5);
    CHECK(fromCString.length() == 5);
    CHECK(!fromCString.empty());

    const char* nullString = nullptr;
    SV fromNull(nullString);
    CHECK(fromNull.empty());

    const std::string str = "world";
    SV fromStdString(str);
    CHECK(fromStdString.size() == 5);
    CHECK(fromStdString.data() == str.data());
}

// ----------------------------------------------------------------------
// Acesso a elementos
// ----------------------------------------------------------------------

void testElementAccess() {
    SV sv("abc");

    CHECK(sv[0] == 'a');
    CHECK(sv[2] == 'c');
    CHECK(sv.front() == 'a');
    CHECK(sv.back() == 'c');

    CHECK(sv.at(1) == 'b');
    CHECK_THROWS_OUT_OF_RANGE(sv.at(3));
    CHECK_THROWS_OUT_OF_RANGE(SV().at(0));
}

// ----------------------------------------------------------------------
// Iteração
// ----------------------------------------------------------------------

void testIteration() {
    SV sv("abc");

    std::string forward;
    for (char c : sv) forward += c;
    CHECK(forward == "abc");

    std::string backward;
    for (auto it = sv.rbegin(); it != sv.rend(); ++it) backward += *it;
    CHECK(backward == "cba");

    CHECK(sv.cend() - sv.cbegin() == 3);
    CHECK(SV().begin() == SV().end());
}

// ----------------------------------------------------------------------
// Subviews
// ----------------------------------------------------------------------

void testSubstr() {
    SV sv("hello world");

    CHECK(sv.substr() == SV("hello world"));
    CHECK(sv.substr(6) == SV("world"));
    CHECK(sv.substr(0, 5) == SV("hello"));
    CHECK(sv.substr(6, 100) == SV("world"));  // count é limitado ao disponível

    // Não copia: aponta para a mesma memória.
    CHECK(sv.substr(6).data() == sv.data() + 6);

    CHECK(sv.substr(sv.size()).empty());      // pos == size é válido
    CHECK_THROWS_OUT_OF_RANGE(sv.substr(sv.size() + 1));
}

// ----------------------------------------------------------------------
// Busca
// ----------------------------------------------------------------------

void testFindView() {
    SV sv("abcabc");

    CHECK(sv.find(SV("abc")) == 0);
    CHECK(sv.find(SV("bc")) == 1);
    CHECK(sv.find(SV("abc"), 1) == 3);       // respeita pos
    CHECK(sv.find(SV("xyz")) == SV::npos);
    CHECK(sv.find(SV("abcabcabc")) == SV::npos);  // needle maior que a view

    CHECK(sv.find(SV(""), 2) == 2);          // needle vazia retorna pos
    CHECK(sv.find(SV(""), 99) == SV::npos);  // pos > size
    CHECK(sv.find(SV("c"), 5) == 5);         // última posição possível
}

void testFindChar() {
    SV sv("abcabc");

    CHECK(sv.find('a') == 0);
    CHECK(sv.find('c') == 2);
    CHECK(sv.find('a', 1) == 3);
    CHECK(sv.find('z') == SV::npos);
    CHECK(sv.find('a', 99) == SV::npos);
}

// ----------------------------------------------------------------------
// Comparação
// ----------------------------------------------------------------------

void testCompareAndOperators() {
    CHECK(SV("abc").compare(SV("abc")) == 0);
    CHECK(SV("abc").compare(SV("abd")) < 0);
    CHECK(SV("abd").compare(SV("abc")) > 0);
    CHECK(SV("abc").compare(SV("abcd")) < 0);  // prefixo é menor
    CHECK(SV("abcd").compare(SV("abc")) > 0);
    CHECK(SV().compare(SV()) == 0);            // duas vazias (ponteiros nulos)
    CHECK(SV().compare(SV("a")) < 0);

    CHECK(SV("abc") == SV("abc"));
    CHECK(SV("abc") != SV("abd"));
    CHECK(SV("abc") != SV("ab"));
    CHECK(SV("abc") < SV("abd"));
    CHECK(SV("abc") <= SV("abc"));
    CHECK(SV("abd") > SV("abc"));
    CHECK(SV("abc") >= SV("abc"));
}

}  // namespace

int main() {
    testConstruction();
    testElementAccess();
    testIteration();
    testSubstr();
    testFindView();
    testFindChar();
    testCompareAndOperators();

    if (g_failures == 0) {
        std::cout << "Todos os testes passaram (" << g_checks << " verificacoes).\n";
        return 0;
    }
    std::cerr << g_failures << " de " << g_checks << " verificacoes falharam.\n";
    return 1;
}
