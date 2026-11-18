// Testes simples para DynamicArray.hpp / DynamicArray.cpp, sem frameworks externos.
//
// Compilação (ajuste os caminhos de include conforme o seu projeto; o
// StringView.cpp é necessário porque ArrayView é construído a partir de const char*):
//   g++ -std=c++17 -Wall -Wextra -I<raiz_do_projeto> DynamicArrayTests.cpp
//       DynamicArray.cpp StringView.cpp -o dynamic_array_tests

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include "DynamicArray.hpp"

// O namespace e a classe se chamam DynamicArray: usamos um alias para evitar ambiguidade.
template <typename T>
using Array = DynamicArray::DynamicArray<T>;
using DynamicArray::ArrayView;
using DynamicArray::BoyerMooreTables;
using DynamicArray::CharArray;
using DynamicArray::KMPTable;
using DynamicArray::LevenshteinMatrix;
using DynamicArray::Match;
using DynamicArray::MatchArray;
using DynamicArray::Matrix;
using DynamicArray::RabinKarpTable;
namespace Tables = DynamicArray::Tables;
namespace Text = DynamicArray::Text;

// ----------------------------------------------------------------------------
// Mini framework
// ----------------------------------------------------------------------------

namespace {

int g_checks = 0;
int g_failures = 0;

#define CHECK(condition)                                                            \
    do {                                                                            \
        ++g_checks;                                                                 \
        if (!(condition)) {                                                         \
            ++g_failures;                                                           \
            std::cerr << "  FALHOU " << __FILE__ << ":" << __LINE__ << "  " #condition \
                      << "\n";                                                      \
        }                                                                           \
    } while (false)

#define CHECK_THROWS(expression, ExceptionType)                                     \
    do {                                                                            \
        ++g_checks;                                                                 \
        bool thrown = false;                                                        \
        try {                                                                       \
            (void)(expression);                                                     \
        } catch (const ExceptionType&) {                                            \
            thrown = true;                                                          \
        } catch (...) {                                                             \
        }                                                                           \
        if (!thrown) {                                                              \
            ++g_failures;                                                           \
            std::cerr << "  FALHOU " << __FILE__ << ":" << __LINE__ << "  " #expression \
                      << " deveria lancar " #ExceptionType "\n";                    \
        }                                                                           \
    } while (false)

std::vector<std::pair<std::string, void (*)()>>& registry() {
    static std::vector<std::pair<std::string, void (*)()>> tests;
    return tests;
}

struct Registrar {
    Registrar(const char* name, void (*fn)()) { registry().emplace_back(name, fn); }
};

#define TEST(name)                                  \
    static void name();                             \
    static Registrar registrar_##name(#name, name); \
    static void name()

}  // namespace

// ----------------------------------------------------------------------------
// 1. Array<T>
// ----------------------------------------------------------------------------

TEST(construcao_basica) {
    Array<int> vazio;
    CHECK(vazio.empty());
    CHECK(vazio.size() == 0);
    CHECK(vazio.capacity() == 0);

    Array<int> zeros(3);
    CHECK(zeros.size() == 3);
    CHECK(zeros[0] == 0 && zeros[2] == 0);

    Array<int> setes(4, 7);
    CHECK(setes.size() == 4);
    CHECK(setes[3] == 7);

    Array<int> lista{1, 2, 3};
    CHECK(lista.size() == 3);
    CHECK(lista[1] == 2);
}

TEST(copia_e_movimento) {
    Array<std::string> original{"a", "b", "c"};

    Array<std::string> copia(original);
    copia[0] = "z";
    CHECK(original[0] == "a");  // copia profunda
    CHECK(copia.size() == 3);

    Array<std::string> movido(std::move(original));
    CHECK(movido.size() == 3);
    CHECK(original.empty());  // NOLINT: estado definido pela implementacao
    CHECK(original.capacity() == 0);

    Array<std::string> atribuido;
    atribuido = copia;
    CHECK(atribuido == copia);

    atribuido = std::move(movido);
    CHECK(atribuido.size() == 3);
    CHECK(movido.empty());  // NOLINT

    Array<std::string>& self = atribuido;
    atribuido = self;  // auto-atribuicao segura
    CHECK(atribuido.size() == 3);
}

TEST(push_back_e_crescimento) {
    Array<int> a;
    a.push_back(1);
    CHECK(a.capacity() == Array<int>::kDefaultCapacity);

    for (int i = 2; i <= 9; ++i) a.push_back(i);
    CHECK(a.size() == 9);
    CHECK(a.capacity() == Array<int>::kDefaultCapacity * Array<int>::kGrowthFactor);
    for (int i = 0; i < 9; ++i) CHECK(a[static_cast<std::size_t>(i)] == i + 1);
}

TEST(emplace_back_retorna_referencia) {
    Array<Match> matches;
    Match& m = matches.emplace_back(3, 5);
    CHECK(m.position == 3 && m.length == 5);
    CHECK(&m == &matches.back());
}

TEST(push_back_de_elemento_do_proprio_array) {
    Array<std::string> a(8, "x");  // capacidade cheia: o proximo push realoca
    a.push_back(a[0]);
    CHECK(a.size() == 9);
    CHECK(a[8] == "x");
}

TEST(acesso_e_excecoes) {
    Array<int> a{10, 20, 30};
    CHECK(a.at(1) == 20);
    CHECK(a.front() == 10);
    CHECK(a.back() == 30);
    CHECK_THROWS(a.at(3), std::out_of_range);

    const Array<int> c{1};
    CHECK(c.at(0) == 1);
    CHECK_THROWS(c.at(1), std::out_of_range);

    Array<int> vazio;
    CHECK_THROWS(vazio.front(), std::out_of_range);
    CHECK_THROWS(vazio.back(), std::out_of_range);
}

TEST(iteradores) {
    Array<int> a{1, 2, 3, 4};
    int soma = 0;
    for (int v : a) soma += v;
    CHECK(soma == 10);
    CHECK(a.end() - a.begin() == 4);
    CHECK(a.cend() - a.cbegin() == 4);
}

TEST(pop_back) {
    Array<int> a{1, 2};
    a.pop_back();
    CHECK(a.size() == 1 && a.back() == 1);
    a.pop_back();
    CHECK(a.empty());
    CHECK_THROWS(a.pop_back(), std::out_of_range);
}

TEST(insert) {
    Array<int> a{1, 3};
    a.insert(1, 2);   // meio
    a.insert(0, 0);   // inicio
    a.insert(4, 4);   // fim (index == size)
    CHECK(a == (Array<int>{0, 1, 2, 3, 4}));
    CHECK_THROWS(a.insert(6, 9), std::out_of_range);
}

TEST(erase) {
    Array<int> a{0, 1, 2, 3};
    a.erase(1);
    CHECK(a == (Array<int>{0, 2, 3}));
    a.erase(2);
    CHECK(a == (Array<int>{0, 2}));
    CHECK_THROWS(a.erase(2), std::out_of_range);
}

TEST(append) {
    Array<int> a{1, 2};
    const int extra[] = {3, 4};
    a.append(extra, 2);
    CHECK(a == (Array<int>{1, 2, 3, 4}));

    a.append(Array<int>{5});
    CHECK(a.size() == 5 && a.back() == 5);

    a.append(nullptr, 0);  // count == 0 e aceito
    CHECK(a.size() == 5);
    CHECK_THROWS(a.append(nullptr, 1), std::invalid_argument);
}

TEST(append_de_faixa_do_proprio_array) {
    Array<int> a{1, 2, 3};  // capacidade 3: forca realocacao
    a.append(a.data(), a.size());
    CHECK(a == (Array<int>{1, 2, 3, 1, 2, 3}));
}

TEST(reserve_e_shrink_to_fit) {
    Array<int> a{1, 2, 3};
    a.reserve(50);
    CHECK(a.capacity() == 50);
    a.reserve(10);  // nunca diminui
    CHECK(a.capacity() == 50);
    CHECK(a.size() == 3);

    a.shrink_to_fit();
    CHECK(a.capacity() == 3);
    CHECK(a[2] == 3);

    a.clear();
    a.shrink_to_fit();
    CHECK(a.capacity() == 0);
    CHECK(a.data() == nullptr);

    CHECK_THROWS(a.reserve(Array<int>::max_size() + 1), std::length_error);
}

TEST(resize) {
    Array<int> a{1, 2, 3};
    a.resize(5);
    CHECK(a.size() == 5 && a[3] == 0 && a[4] == 0);
    a.resize(7, 9);
    CHECK(a[5] == 9 && a[6] == 9);
    a.resize(2);
    CHECK(a == (Array<int>{1, 2}));
}

TEST(clear_mantem_capacidade) {
    Array<std::string> a{"a", "b", "c"};
    const std::size_t capacidade = a.capacity();
    a.clear();
    CHECK(a.empty());
    CHECK(a.capacity() == capacidade);
}

TEST(swap_e_comparacao) {
    Array<int> a{1, 2};
    Array<int> b{3};
    swap(a, b);
    CHECK(a == (Array<int>{3}));
    CHECK(b == (Array<int>{1, 2}));
    CHECK(a != b);
    CHECK(!(a == b));
}

// ----------------------------------------------------------------------------
// 2. Text
// ----------------------------------------------------------------------------

TEST(texto_make_append_to_string) {
    CharArray texto = Text::make_char_array("abc");
    CHECK(texto.size() == 3);
    Text::append_text(texto, "def");
    CHECK(Text::to_string(texto) == "abcdef");
    CHECK(Text::to_string(CharArray()).empty());
}

TEST(texto_as_view) {
    CharArray texto = Text::make_char_array("hello");
    ArrayView view = Text::as_view(texto);
    CHECK(view.size() == 5);
    CHECK(view.data() == texto.data());
}

TEST(texto_substring) {
    const CharArray texto = Text::make_char_array("abcdef");
    CHECK(Text::to_string(Text::substring(texto, 2, 3)) == "cde");
    CHECK(Text::to_string(Text::substring(texto, 2)) == "cdef");      // ate o fim
    CHECK(Text::to_string(Text::substring(texto, 4, 100)) == "ef");   // length limitado
    CHECK(Text::substring(texto, 6).empty());                          // position == size
    CHECK_THROWS(Text::substring(texto, 7), std::out_of_range);
}

// ----------------------------------------------------------------------------
// 3. Matrix<T>
// ----------------------------------------------------------------------------

TEST(matrix_construcao_e_acesso) {
    Matrix<int> vazia;
    CHECK(vazia.empty() && vazia.rows() == 0 && vazia.cols() == 0);

    Matrix<int> m(2, 3, 5);
    CHECK(m.rows() == 2 && m.cols() == 3 && m.size() == 6);
    CHECK(m(1, 2) == 5);

    m(0, 1) = 8;
    CHECK(m.at(0, 1) == 8);
    CHECK_THROWS(m.at(2, 0), std::out_of_range);
    CHECK_THROWS(m.at(0, 3), std::out_of_range);
}

TEST(matrix_assign_fill_e_movimento) {
    Matrix<int> m(2, 2, 1);
    m.assign(3, 4, 2);
    CHECK(m.rows() == 3 && m.cols() == 4 && m.size() == 12);
    CHECK(m(2, 3) == 2);

    m.fill(9);
    CHECK(m(0, 0) == 9 && m(2, 3) == 9);

    Matrix<int> movida(std::move(m));
    CHECK(movida.rows() == 3 && movida(1, 1) == 9);
    CHECK(m.rows() == 0 && m.cols() == 0 && m.empty());  // NOLINT

    const std::size_t grande = std::numeric_limits<std::size_t>::max();
    CHECK_THROWS(movida.assign(grande, 2), std::length_error);
}

// ----------------------------------------------------------------------------
// 4. Match
// ----------------------------------------------------------------------------

TEST(match_igualdade) {
    CHECK(Match() == Match(0, 0));
    CHECK(Match(1, 2) == Match(1, 2));
    CHECK(Match(1, 2) != Match(1, 3));

    MatchArray resultados;
    resultados.emplace_back(4, 2);
    CHECK(resultados[0] == Match(4, 2));
}

// ----------------------------------------------------------------------------
// 5. Tabelas auxiliares
// ----------------------------------------------------------------------------

TEST(tabela_kmp) {
    KMPTable t = Tables::make_kmp_table(5);
    CHECK(t.failure.size() == 5);
    CHECK(t.failure[0] == 0 && t.failure[4] == 0);
    CHECK(Tables::make_kmp_table(0).failure.empty());
}

TEST(tabelas_boyer_moore) {
    BoyerMooreTables t = Tables::make_boyer_moore_tables(4);
    CHECK(t.bad_character.size() == BoyerMooreTables::kAlphabetSize);
    CHECK(t.bad_character[0] == -1 && t.bad_character[255] == -1);
    CHECK(t.good_suffix_shift.size() == 5);
    CHECK(t.suffix_border.size() == 5);
    CHECK(BoyerMooreTables::index_of(static_cast<char>(-1)) == 255);

    CHECK_THROWS(Tables::make_boyer_moore_tables(std::numeric_limits<std::size_t>::max()),
                 std::length_error);
}

TEST(tabela_rabin_karp) {
    RabinKarpTable t = Tables::make_rabin_karp_table(10, 3);
    CHECK(t.window_hashes.size() == 8);          // 10 - 3 + 1
    CHECK(t.high_order_factor == 256ULL * 256ULL);  // base^(m-1)
    CHECK(t.base == 256 && t.modulus == 1'000'000'007ULL);

    CHECK(Tables::make_rabin_karp_table(2, 5).window_hashes.empty());  // padrao maior que o texto
    CHECK(Tables::make_rabin_karp_table(4, 0).high_order_factor == 1);
    CHECK(Tables::make_rabin_karp_table(4, 2, 300, 7).base == 300 % 7);

    CHECK_THROWS(Tables::make_rabin_karp_table(4, 2, 256, 0), std::invalid_argument);
    CHECK_THROWS(Tables::make_rabin_karp_table(4, 2, 256, (std::uint64_t{1} << 32) + 1),
                 std::invalid_argument);
}

TEST(matriz_levenshtein) {
    LevenshteinMatrix m = Tables::make_levenshtein_matrix(3, 4);
    CHECK(m.rows() == 4 && m.cols() == 5);
    CHECK(m(0, 0) == 0 && m(3, 4) == 0);

    CHECK(Tables::make_levenshtein_matrix(0, 0).size() == 1);

    const std::size_t grande = std::numeric_limits<std::size_t>::max();
    CHECK_THROWS(Tables::make_levenshtein_matrix(grande, 1), std::length_error);
    CHECK_THROWS(Tables::make_levenshtein_matrix(1, grande), std::length_error);
}

// ----------------------------------------------------------------------------
// Execução
// ----------------------------------------------------------------------------

int main() {
    for (const auto& [name, fn] : registry()) {
        const int falhasAntes = g_failures;
        try {
            fn();
        } catch (const std::exception& e) {
            ++g_failures;
            std::cerr << "  EXCECAO INESPERADA: " << e.what() << "\n";
        } catch (...) {
            ++g_failures;
            std::cerr << "  EXCECAO INESPERADA (desconhecida)\n";
        }
        std::cout << (g_failures == falhasAntes ? "[OK]    " : "[FALHA] ") << name << "\n";
    }

    std::cout << "\n" << registry().size() << " testes, " << g_checks << " verificacoes, "
              << g_failures << " falha(s)\n";
    return g_failures == 0 ? 0 : 1;
}
