#include <iostream>
#include <stdexcept>
#include <string>

#include "src/Main/Model/Structures/String/Normalization/Normalizer.hpp"

namespace {

int g_failures = 0;
int g_total = 0;

#define CHECK(cond)                                                              \
    do {                                                                         \
        ++g_total;                                                               \
        if (!(cond)) {                                                           \
            ++g_failures;                                                        \
            std::cerr << "  FALHOU (linha " << __LINE__ << "): " #cond << "\n";  \
        }                                                                        \
    } while (0)

#define CHECK_THROWS(expr)                                                       \
    do {                                                                         \
        ++g_total;                                                               \
        bool threw = false;                                                      \
        try { (void)(expr); } catch (const std::invalid_argument&) { threw = true; } \
        if (!threw) {                                                            \
            ++g_failures;                                                        \
            std::cerr << "  FALHOU (linha " << __LINE__ << "): esperava invalid_argument em " #expr << "\n"; \
        }                                                                        \
    } while (0)

using Normalizer::Normalizer;
using Normalizer::UnicodeForm;

// Atalho: executa a operação e devolve o resultado como std::string.
std::string toStr(const NormalizerBuffer& b) { return b.str(); }

void testNormalizeUnicode() {
    std::cout << "normalizeUnicode\n";
    const std::string composed = "\xC3\xA9";          // é (U+00E9)
    const std::string decomposed = "e\xCC\x81";       // e + U+0301

    Normalizer nfc(UnicodeForm::NFC);
    Normalizer nfd(UnicodeForm::NFD);

    CHECK(toStr(nfc.normalizeUnicode(decomposed)) == composed);
    CHECK(toStr(nfc.normalizeUnicode(composed)) == composed);
    CHECK(toStr(nfd.normalizeUnicode(composed)) == decomposed);
    CHECK(toStr(nfc.normalizeUnicode("abc")) == "abc");
    CHECK(toStr(nfc.normalizeUnicode("")).empty());
}

void testCaseFold() {
    std::cout << "caseFold\n";
    Normalizer n;

    CHECK(toStr(n.caseFold("ABC xyz")) == "abc xyz");
    CHECK(toStr(n.caseFold("\xC3\x87")) == "\xC3\xA7");  // Ç -> ç
    CHECK(toStr(n.caseFold("\xC3\x9F")) == "ss");        // ß -> ss
    CHECK(toStr(n.caseFold("123!")) == "123!");
}

void testRemoveDiacritics() {
    std::cout << "removeDiacritics\n";
    Normalizer n;

    CHECK(toStr(n.removeDiacritics("a\xC3\xA7\xC3\xA3o")) == "acao");  // ação
    CHECK(toStr(n.removeDiacritics("\xC3\x89\xC3\xA9")) == "Ee");      // Éé
    CHECK(toStr(n.removeDiacritics("e\xCC\x81")) == "e");              // e + acento combinante
    CHECK(toStr(n.removeDiacritics("abc")) == "abc");
}

void testNormalize() {
    std::cout << "normalize\n";
    Normalizer nfc(UnicodeForm::NFC);
    Normalizer nfd(UnicodeForm::NFD);

    // Á maiúsculo -> a minúsculo sem acento
    CHECK(toStr(nfc.normalize("\xC3\x81RVORE")) == "arvore");
    CHECK(toStr(nfd.normalize("\xC3\x81RVORE")) == "arvore");
    CHECK(toStr(nfc.normalize("")).empty());
}

void testEquivalent() {
    std::cout << "equivalent\n";
    Normalizer n;

    CHECK(n.equivalent("Caf\xC3\xA9", "CAFE\xCC\x81"));  // composto vs decomposto + caixa
    CHECK(n.equivalent("Caf\xC3\xA9", "cafe"));          // acento ignorado
    CHECK(n.equivalent("", ""));
    CHECK(!n.equivalent("casa", "caso"));
}

void testInvalidUtf8() {
    std::cout << "UTF-8 invalido\n";
    Normalizer n;

    CHECK_THROWS(n.normalize("\xFF"));              // byte inicial invalido
    CHECK_THROWS(n.caseFold("\xC3"));               // sequencia truncada
    CHECK_THROWS(n.removeDiacritics("\xC3\x28"));   // continuacao invalida
    CHECK_THROWS(n.normalizeUnicode("\xC0\x80"));   // sequencia overlong
    CHECK_THROWS(n.equivalent("ok", "\xED\xA0\x80")); // surrogate
}

} // namespace

int main() {
    testNormalizeUnicode();
    testCaseFold();
    testRemoveDiacritics();
    testNormalize();
    testEquivalent();
    testInvalidUtf8();

    std::cout << "\n" << (g_total - g_failures) << "/" << g_total << " verificacoes passaram\n";
    return g_failures == 0 ? 0 : 1;
}
