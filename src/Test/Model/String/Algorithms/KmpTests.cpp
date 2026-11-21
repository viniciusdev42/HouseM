#include <cstddef>
#include <iostream>

#include "src/Main/Model/Structures/String/Algorithms/Kmp/Kmp.hpp"

namespace {

int g_total = 0;
int g_failed = 0;

#define CHECK(condition)                                                        \
    do {                                                                        \
        ++g_total;                                                              \
        if (!(condition)) {                                                     \
            ++g_failed;                                                         \
            std::cerr << "  FALHOU (linha " << __LINE__ << "): " #condition     \
                      << std::endl;                                             \
        }                                                                       \
    } while (false)

using Result = DynamicK;  // DynamicArray<std::size_t>

void report(const char* name, int failedBefore) {
    std::cout << (g_failed == failedBefore ? "[OK]    " : "[FALHA] ") << name
              << std::endl;
}

// ----------------------------------------------------------------------------
// computeLPS
// ----------------------------------------------------------------------------

void testLpsEmptyPattern() {
    Result lps = KMP::computeLPS(KView(""));
    CHECK(lps.empty());
}

void testLpsSingleChar() {
    CHECK(KMP::computeLPS(KView("a")) == (Result{0}));
}

void testLpsNoRepeatedPrefix() {
    CHECK(KMP::computeLPS(KView("abcd")) == (Result{0, 0, 0, 0}));
}

void testLpsAllEqualChars() {
    CHECK(KMP::computeLPS(KView("aaaa")) == (Result{0, 1, 2, 3}));
}

void testLpsRepeatedBlock() {
    CHECK(KMP::computeLPS(KView("abab")) == (Result{0, 0, 1, 2}));
}

void testLpsFallbackCase() {
    // Exercita o recuo via lps[prefixLength - 1].
    CHECK(KMP::computeLPS(KView("aabaaab")) == (Result{0, 1, 0, 1, 2, 2, 3}));
}

// ----------------------------------------------------------------------------
// search
// ----------------------------------------------------------------------------

void testSearchSingleMatch() {
    CHECK(KMP::search(KView("abxabcabcaby"), KView("abcaby")) == (Result{6}));
}

void testSearchMatchAtStartAndEnd() {
    CHECK(KMP::search(KView("abcxxabc"), KView("abc")) == (Result{0, 5}));
}

void testSearchMultipleMatches() {
    CHECK(KMP::search(KView("abcabcabc"), KView("abc")) == (Result{0, 3, 6}));
}

void testSearchOverlappingMatches() {
    CHECK(KMP::search(KView("aaaaa"), KView("aa")) == (Result{0, 1, 2, 3}));
    CHECK(KMP::search(KView("ababa"), KView("aba")) == (Result{0, 2}));
}

void testSearchNoMatch() {
    CHECK(KMP::search(KView("abcdef"), KView("xyz")).empty());
    CHECK(KMP::search(KView("aaaa"), KView("aaab")).empty());
}

void testSearchSingleCharPattern() {
    CHECK(KMP::search(KView("banana"), KView("a")) == (Result{1, 3, 5}));
}

void testSearchPatternEqualsText() {
    CHECK(KMP::search(KView("abc"), KView("abc")) == (Result{0}));
}

void testSearchPatternLongerThanText() {
    CHECK(KMP::search(KView("ab"), KView("abc")).empty());
}

void testSearchEmptyInputs() {
    CHECK(KMP::search(KView(""), KView("a")).empty());
    CHECK(KMP::search(KView("abc"), KView("")).empty());
    CHECK(KMP::search(KView(""), KView("")).empty());
}

void testSearchIsCaseSensitive() {
    CHECK(KMP::search(KView("ABC abc"), KView("abc")) == (Result{4}));
}

}  // namespace

#define RUN(test)                      \
    do {                               \
        int before = g_failed;         \
        test();                        \
        report(#test, before);         \
    } while (false)

int main() {
    // computeLPS
    RUN(testLpsEmptyPattern);
    RUN(testLpsSingleChar);
    RUN(testLpsNoRepeatedPrefix);
    RUN(testLpsAllEqualChars);
    RUN(testLpsRepeatedBlock);
    RUN(testLpsFallbackCase);

    // search
    RUN(testSearchSingleMatch);
    RUN(testSearchMatchAtStartAndEnd);
    RUN(testSearchMultipleMatches);
    RUN(testSearchOverlappingMatches);
    RUN(testSearchNoMatch);
    RUN(testSearchSingleCharPattern);
    RUN(testSearchPatternEqualsText);
    RUN(testSearchPatternLongerThanText);
    RUN(testSearchEmptyInputs);
    RUN(testSearchIsCaseSensitive);

    std::cout << "\n" << (g_total - g_failed) << "/" << g_total
              << " verificacoes passaram." << std::endl;
    return g_failed == 0 ? 0 : 1;
}
