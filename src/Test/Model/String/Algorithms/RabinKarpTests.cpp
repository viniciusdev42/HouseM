#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>

#include "src/Main/Model/Structures/String/Algorithms/Karp/RabinKarp.hpp"

namespace {

int g_checks = 0;
int g_failures = 0;

#define CHECK(cond)                                                              \
    do {                                                                         \
        ++g_checks;                                                              \
        if (!(cond)) {                                                           \
            ++g_failures;                                                        \
            std::cerr << "  FALHOU (linha " << __LINE__ << "): " #cond << '\n'; \
        }                                                                        \
    } while (false)

#define RUN(test)                                    \
    do {                                             \
        std::cout << "[ RUN ] " #test << '\n';       \
        test();                                      \
    } while (false)

// Cria uma view sobre um literal (sem o '\0').
template <std::size_t N>
KarpView view(const char (&literal)[N]) {
    return KarpView(literal, N - 1);
}

// Compara o resultado de find_all com as posicoes esperadas.
bool positions_equal(const KarpArray& actual, std::initializer_list<std::size_t> expected) {
    if (actual.size() != expected.size()) return false;
    std::size_t i = 0;
    for (std::size_t value : expected) {
        if (actual[i++] != value) return false;
    }
    return true;
}

// ---------------------------------------------------------------- to_value

void test_to_value() {
    CHECK(RabinKarp::to_value('a') == 97);
    CHECK(RabinKarp::to_value('\0') == 0);
    // char negativo deve ser tratado como unsigned char.
    CHECK(RabinKarp::to_value(static_cast<char>(0xFF)) == 255);
}

// --------------------------------------------------------------- power_mod

void test_power_mod() {
    CHECK(RabinKarp::power_mod(0) == 1);
    CHECK(RabinKarp::power_mod(1) == 256);
    CHECK(RabinKarp::power_mod(2) == 65536);
    // 256^4 = 4294967296; mod 1'000'000'007 = 294967268.
    CHECK(RabinKarp::power_mod(4) == 294967268ULL);
    CHECK(RabinKarp::power_mod(100) < RabinKarp::kModulus);
}

// ----------------------------------------------------------------- hash_of

void test_hash_of() {
    CHECK(RabinKarp::hash_of(view(""), 0) == 0);
    CHECK(RabinKarp::hash_of(view("a"), 1) == 97);
    CHECK(RabinKarp::hash_of(view("ab"), 2) == 97ULL * 256 + 98);
    // Usa apenas os 'length' primeiros caracteres.
    CHECK(RabinKarp::hash_of(view("abc"), 2) == RabinKarp::hash_of(view("ab"), 2));
    // Strings iguais -> hashes iguais.
    CHECK(RabinKarp::hash_of(view("hello"), 5) == RabinKarp::hash_of(view("hello"), 5));
}

// ---------------------------------------------------------------- find_all

void test_find_all_basic() {
    CHECK(positions_equal(RabinKarp::find_all(view("abracadabra"), view("abra")), {0, 7}));
    CHECK(positions_equal(RabinKarp::find_all(view("abcabc"), view("c")), {2, 5}));
}

void test_find_all_overlapping() {
    CHECK(positions_equal(RabinKarp::find_all(view("aaaa"), view("aa")), {0, 1, 2}));
}

void test_find_all_no_match() {
    CHECK(RabinKarp::find_all(view("abcdef"), view("xyz")).empty());
    // Padrao maior que o texto.
    CHECK(RabinKarp::find_all(view("ab"), view("abc")).empty());
    // Texto vazio.
    CHECK(RabinKarp::find_all(view(""), view("a")).empty());
}

void test_find_all_whole_text_and_edges() {
    CHECK(positions_equal(RabinKarp::find_all(view("abc"), view("abc")), {0}));
    CHECK(positions_equal(RabinKarp::find_all(view("xxabc"), view("abc")), {2}));
    CHECK(positions_equal(RabinKarp::find_all(view("abcxx"), view("abc")), {0}));
}

// -------------------------------------------------------------- find_first

void test_find_first() {
    auto first = RabinKarp::find_first(view("abracadabra"), view("abra"));
    CHECK(first.has_value());
    CHECK(first.value_or(999) == 0);

    auto middle = RabinKarp::find_first(view("xxabcabc"), view("abc"));
    CHECK(middle.has_value());
    CHECK(middle.value_or(999) == 2);

    auto last = RabinKarp::find_first(view("xxxa"), view("a"));
    CHECK(last.has_value());
    CHECK(last.value_or(999) == 3);
}

void test_find_first_no_match() {
    CHECK(!RabinKarp::find_first(view("abcdef"), view("xyz")).has_value());
    CHECK(!RabinKarp::find_first(view("ab"), view("abc")).has_value());
    CHECK(!RabinKarp::find_first(view(""), view("a")).has_value());
}

void test_find_first_agrees_with_find_all() {
    const KarpView text = view("the quick brown fox jumps over the lazy dog");
    const KarpView pattern = view("the");
    KarpArray all = RabinKarp::find_all(text, pattern);
    auto first = RabinKarp::find_first(text, pattern);
    CHECK(!all.empty());
    CHECK(first.has_value());
    CHECK(first.value_or(999) == all[0]);
}

}  // namespace

int main() {
    RUN(test_to_value);
    RUN(test_power_mod);
    RUN(test_hash_of);
    RUN(test_find_all_basic);
    RUN(test_find_all_overlapping);
    RUN(test_find_all_no_match);
    RUN(test_find_all_whole_text_and_edges);
    RUN(test_find_first);
    RUN(test_find_first_no_match);
    RUN(test_find_first_agrees_with_find_all);

    std::cout << '\n' << (g_checks - g_failures) << "/" << g_checks << " verificacoes passaram.\n";
    return g_failures == 0 ? 0 : 1;
}
