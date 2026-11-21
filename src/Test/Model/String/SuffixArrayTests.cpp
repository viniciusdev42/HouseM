#include <algorithm>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

#include "src/Main/Model/Structures/String/Indexing/SuffixArray.hpp"

namespace {

int g_failures = 0;
int g_checks = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        ++g_checks;                                                          \
        if (!(cond)) {                                                       \
            ++g_failures;                                                    \
            std::cerr << "  FALHOU (linha " << __LINE__ << "): " #cond "\n"; \
        }                                                                    \
    } while (0)

using Vec = std::vector<int>;

// A string precisa continuar viva enquanto o SuffixArray for usado.
SuffixView view(const std::string& s) { return SuffixView(s.data(), s.size()); }

// Referência ingênua: ordena os sufixos comparando as strings diretamente.
// Usa comparação sem sinal (como o SuffixArray, que usa unsigned char).
Vec bruteForceSA(const std::string& s) {
    Vec sa(s.size());
    std::iota(sa.begin(), sa.end(), 0);
    std::sort(sa.begin(), sa.end(), [&](int a, int b) {
        const std::string x = s.substr(a), y = s.substr(b);
        return std::lexicographical_compare(
            x.begin(), x.end(), y.begin(), y.end(),
            [](char l, char r) {
                return static_cast<unsigned char>(l) < static_cast<unsigned char>(r);
            });
    });
    return sa;
}

int bruteForceLcp(const std::string& s, int a, int b) {
    int h = 0;
    while (a + h < (int)s.size() && b + h < (int)s.size() && s[a + h] == s[b + h]) ++h;
    return h;
}

void run(const char* name, void (*test)()) {
    const int before = g_failures;
    test();
    std::cout << (g_failures == before ? "[OK]    " : "[FALHA] ") << name << "\n";
}

// ------------------------------------------------------------------

void testEmptyText() {
    std::string s;
    SuffixArray sa(view(s));
    CHECK(sa.size() == 0);
    CHECK(sa.suffixArray().empty());
    CHECK(sa.rank().empty());
    CHECK(sa.lcp().empty());
}

void testSingleCharacter() {
    std::string s = "a";
    SuffixArray sa(view(s));
    CHECK(sa.size() == 1);
    CHECK(sa[0] == 0);
    CHECK(sa.rank() == Vec({0}));
    CHECK(sa.lcp() == Vec({0}));
}

void testBanana() {
    std::string s = "banana";
    SuffixArray sa(view(s));

    CHECK(sa.size() == 6);
    CHECK(sa.suffixArray() == Vec({5, 3, 1, 0, 4, 2}));  // a, ana, anana, banana, na, nana
    CHECK(sa.rank() == Vec({3, 2, 5, 1, 4, 0}));
    CHECK(sa.lcp() == Vec({0, 1, 3, 0, 0, 2}));
}

void testAllEqualCharacters() {
    std::string s = "aaaa";
    SuffixArray sa(view(s));

    CHECK(sa.suffixArray() == Vec({3, 2, 1, 0}));
    CHECK(sa.rank() == Vec({3, 2, 1, 0}));
    CHECK(sa.lcp() == Vec({0, 1, 2, 3}));
}

void testOperatorBracketMatchesSuffixArray() {
    std::string s = "mississippi";
    SuffixArray sa(view(s));

    CHECK(sa.size() == s.size());
    for (std::size_t i = 0; i < sa.size(); ++i) {
        CHECK(sa[i] == sa.suffixArray()[i]);
    }
}

void testRankIsInverseOfSuffixArray() {
    std::string s = "abracadabra";
    SuffixArray sa(view(s));

    for (std::size_t i = 0; i < sa.size(); ++i) {
        CHECK(sa.rank()[sa[i]] == static_cast<int>(i));
    }
}

void testAgainstBruteForce() {
    const std::vector<std::string> samples = {
        "mississippi", "abracadabra", "abababab", "zyxwv", "abcabcabc",
        std::string("\xff\x01\xff\x80\x01", 5),  // bytes >= 128 (ordem sem sinal)
    };

    for (const std::string& s : samples) {
        SuffixArray sa(view(s));

        CHECK(sa.suffixArray() == bruteForceSA(s));

        Vec expectedLcp(s.size(), 0);
        for (std::size_t i = 1; i < s.size(); ++i) {
            expectedLcp[i] = bruteForceLcp(s, sa[i], sa[i - 1]);
        }
        CHECK(sa.lcp() == expectedLcp);
    }
}

}  // namespace

int main() {
    run("texto vazio", testEmptyText);
    run("um unico caractere", testSingleCharacter);
    run("banana (valores conhecidos)", testBanana);
    run("caracteres todos iguais", testAllEqualCharacters);
    run("operator[] == suffixArray()[i]", testOperatorBracketMatchesSuffixArray);
    run("rank e o inverso do suffix array", testRankIsInverseOfSuffixArray);
    run("comparacao com forca bruta (SA e LCP)", testAgainstBruteForce);

    std::cout << "\n" << (g_checks - g_failures) << "/" << g_checks << " verificacoes passaram.\n";
    return g_failures == 0 ? 0 : 1;
}
