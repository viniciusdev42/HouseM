#include <cmath>
#include <cstddef>
#include <iostream>

#include "src/Model/Structures/String/Algorithms/Levenshtein/Levenshtein.hpp"

namespace {

int g_total = 0;
int g_failed = 0;

void check(bool condition, const char* expression, int line) {
    ++g_total;
    if (!condition) {
        ++g_failed;
        std::cerr << "FALHOU (linha " << line << "): " << expression << '\n';
    }
}

#define CHECK(expr) check((expr), #expr, __LINE__)
#define CHECK_NEAR(a, b) check(std::fabs((a) - (b)) < 1e-9, #a " ~= " #b, __LINE__)

// ----------------------------------------------------------------------------
// levenshtein_distance
// ----------------------------------------------------------------------------

void test_distance_identical_and_empty() {
    CHECK(Levenshtein::levenshtein_distance("", "") == 0);
    CHECK(Levenshtein::levenshtein_distance("abc", "abc") == 0);
    CHECK(Levenshtein::levenshtein_distance("", "abc") == 3);
    CHECK(Levenshtein::levenshtein_distance("abc", "") == 3);
}

void test_distance_single_operations() {
    CHECK(Levenshtein::levenshtein_distance("abc", "abd") == 1);   // substituicao
    CHECK(Levenshtein::levenshtein_distance("abc", "abcd") == 1);  // insercao
    CHECK(Levenshtein::levenshtein_distance("abcd", "abc") == 1);  // remocao
}

void test_distance_known_cases() {
    CHECK(Levenshtein::levenshtein_distance("kitten", "sitting") == 3);
    CHECK(Levenshtein::levenshtein_distance("flaw", "lawn") == 2);
    CHECK(Levenshtein::levenshtein_distance("sunday", "saturday") == 3);
}

void test_distance_is_symmetric() {
    CHECK(Levenshtein::levenshtein_distance("kitten", "sitting") ==
          Levenshtein::levenshtein_distance("sitting", "kitten"));
    CHECK(Levenshtein::levenshtein_distance("ab", "abcdef") ==
          Levenshtein::levenshtein_distance("abcdef", "ab"));
}

void test_distance_works_with_subviews() {
    const std::string text = "xxkittenxx";
    LevenView view(text.data() + 2, 6);  // "kitten", sem '\0' no final
    CHECK(Levenshtein::levenshtein_distance(view, "sitting") == 3);
}

// ----------------------------------------------------------------------------
// levenshtein_similarity
// ----------------------------------------------------------------------------

void test_similarity() {
    CHECK_NEAR(Levenshtein::levenshtein_similarity("", ""), 1.0);
    CHECK_NEAR(Levenshtein::levenshtein_similarity("abc", "abc"), 1.0);
    CHECK_NEAR(Levenshtein::levenshtein_similarity("abc", "xyz"), 0.0);
    CHECK_NEAR(Levenshtein::levenshtein_similarity("abc", ""), 0.0);
    // distancia 3, maior tamanho 7 -> 1 - 3/7
    CHECK_NEAR(Levenshtein::levenshtein_similarity("kitten", "sitting"), 1.0 - 3.0 / 7.0);
}

// ----------------------------------------------------------------------------
// levenshtein_matrix
// ----------------------------------------------------------------------------

void test_matrix_dimensions_and_borders() {
    const LevenArray matrix = Levenshtein::levenshtein_matrix("abc", "de");
    CHECK(matrix.rows() == 4);
    CHECK(matrix.cols() == 3);
    for (std::size_t i = 0; i <= 3; ++i) CHECK(matrix(i, 0) == i);
    for (std::size_t j = 0; j <= 2; ++j) CHECK(matrix(0, j) == j);
}

void test_matrix_known_values() {
    const LevenArray matrix = Levenshtein::levenshtein_matrix("kitten", "sitting");
    CHECK(matrix(1, 1) == 1);  // "k" -> "s"
    CHECK(matrix(2, 2) == 1);  // "ki" -> "si"
    CHECK(matrix(6, 7) == 3);  // canto final = distancia total
}

void test_matrix_empty_inputs() {
    const LevenArray both_empty = Levenshtein::levenshtein_matrix("", "");
    CHECK(both_empty.rows() == 1 && both_empty.cols() == 1);
    CHECK(both_empty(0, 0) == 0);

    const LevenArray one_empty = Levenshtein::levenshtein_matrix("", "abc");
    CHECK(one_empty.rows() == 1 && one_empty.cols() == 4);
    CHECK(one_empty(0, 3) == 3);
}

void test_matrix_matches_distance() {
    const char* pairs[][2] = {{"kitten", "sitting"}, {"flaw", "lawn"}, {"", "abc"}, {"abc", "abc"}};
    for (const auto& pair : pairs) {
        const LevenView a(pair[0]);
        const LevenView b(pair[1]);
        const LevenArray matrix = Levenshtein::levenshtein_matrix(a, b);
        CHECK(matrix(a.size(), b.size()) == Levenshtein::levenshtein_distance(a, b));
    }
}

}  // namespace

int main() {
    test_distance_identical_and_empty();
    test_distance_single_operations();
    test_distance_known_cases();
    test_distance_is_symmetric();
    test_distance_works_with_subviews();
    test_similarity();
    test_matrix_dimensions_and_borders();
    test_matrix_known_values();
    test_matrix_empty_inputs();
    test_matrix_matches_distance();

    std::cout << (g_total - g_failed) << "/" << g_total << " verificacoes passaram\n";
    return g_failed == 0 ? 0 : 1;
}
