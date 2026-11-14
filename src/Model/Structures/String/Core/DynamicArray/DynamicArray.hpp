#ifndef DYNAMIC_ARRAY
#define DYNAMIC_ARRAY

/* Dynamic Array para armazenar caracteres, resultados de
    operações e tabelas auxiliares de KMP, Boyer-Moore, Rabin-Karp e Levenshtein.

 Organização:
   1. DynamicArray<T>        -> contêiner genérico (núcleo)
   2. CharArray + text::     -> sequências de caracteres
   3. Matrix<T>              -> tabela 2D (Levenshtein) sobre DynamicArray
   4. Resultados             -> Match, MatchArray, IndexArray, DistanceArray
   5. Tabelas auxiliares     -> KMPTable, BoyerMooreTables, RabinKarpTable,
                                LevenshteinMatrix e suas fábricas em tables::

 Este arquivo fornece apenas o armazenamento que eles utilizam.*/

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

#include "src/Model/Structures/String/Core/StringView/StringView.hpp"

using ArrayView = StringView::StringView;

namespace DynamicArray {
  
// ============================================================================
// 1. DynamicArray<T>
// ============================================================================

template <typename T>
class DynamicArray {
public:
    using value_type      = T;
    using size_type       = std::size_t;
    using reference       = T&;
    using const_reference = const T&;
    using pointer         = T*;
    using const_pointer   = const T*;
    using iterator        = T*;
    using const_iterator  = const T*;

    static constexpr size_type kDefaultCapacity = 8;
    static constexpr size_type kGrowthFactor    = 2;

    // --- Construção, cópia e movimentação ----------------------------------

    DynamicArray() noexcept = default;

    // Os construtores abaixo delegam ao padrão: assim, se uma exceção ocorrer
    // no corpo, o objeto já está "construído" e o destrutor libera a memória.
    explicit DynamicArray(size_type count);

    DynamicArray(size_type count, const T& value);
    
    DynamicArray(std::initializer_list<T> values);
    
    DynamicArray(const DynamicArray& other);
    
    DynamicArray(DynamicArray&& other) noexcept;
    
    // Copy-and-swap: garante garantia forte de exceção e trata auto-atribuição.
    DynamicArray& operator=(const DynamicArray& other);
    
    DynamicArray& operator=(DynamicArray&& other) noexcept;
  
    ~DynamicArray();

    // --- Capacidade ---------------------------------------------------------

    size_type size() const noexcept;
    size_type capacity() const noexcept;
    bool empty() const noexcept;

    static size_type max_size() noexcept;

    // Capacidade exata solicitada; nunca diminui.
    void reserve(size_type new_capacity);
    
    void shrink_to_fit();

    // Redimensiona o tamanho lógico; novos elementos são value-initialized.
    void resize(size_type count);
    
    void resize(size_type count, const T& value);
    
    // --- Acesso -------------------------------------------------------------

    // Sem verificação de limites (como std::vector); use at() quando preciso.
    reference operator[](size_type index) noexcept;
    
    const_reference operator[](size_type index) const noexcept;

    reference at(size_type index);
    
    const_reference at(size_type index) const;
    
    reference front();
    const_reference front() const;
    reference back();
    const_reference back() const;
    pointer data() noexcept;
    const_pointer data() const noexcept;

    iterator begin() noexcept;
    iterator end() noexcept;
    const_iterator begin() const noexcept;
    const_iterator end() const noexcept;
    const_iterator cbegin() const noexcept;
    const_iterator cend() const noexcept;

    // --- Modificadores ------------------------------------------------------

    template <typename... Args> reference emplace_back(Args&&... args);

    void push_back(const T& value);
    
    void push_back(T&& value);

    void pop_back();

    // Insere 'value' na posição 'index' (0 <= index <= size()).
    void insert(size_type index, const T& value);

    void erase(size_type index);

    // Anexa 'count' elementos a partir de 'source'. Aceita faixas do próprio array.
    void append(const T* source, size_type count);

    void append(const DynamicArray& other);

    // Destrói os elementos mas mantém a capacidade para reaproveitamento.
    void clear() noexcept;

    void swap(DynamicArray& other) noexcept;
    
    void swap(DynamicArray& a, DynamicArray& b);

    bool operator==(const DynamicArray& a, const DynamicArray& b);
    
    bool operator!=(const DynamicArray& a, const DynamicArray& b);

private:
    pointer data_ = nullptr;
    size_type size_ = 0;
    size_type capacity_ = 0;

    static pointer allocate(size_type count);

    static void deallocate(pointer block, size_type count) noexcept;

    static void destroy_range(pointer first, size_type count) noexcept;
    
    void check_index(size_type index) const;
    
    // Crescimento geométrico (x2) => push_back com custo amortizado O(1).
    size_type grown_capacity(size_type min_required) const;
    
    // Move os elementos para um novo bloco; mantém o array intacto se algo lançar.
    void reallocate(size_type new_capacity);
    
    template <typename Construct>
    void resize_with(size_type count, Construct construct);
    
};

// ============================================================================
// 2. Sequências de caracteres
// ============================================================================

using CharArray = DynamicArray<char>;

// Funções livres em vez de herança: DynamicArray não tem destrutor virtual e
// herdar dele convidaria a erros de uso polimórfico.
namespace Text {

CharArray make_char_array(ArrayView content);

void append_text(CharArray& target, ArrayView content);

ArrayView as_view(const CharArray& array) noexcept;

std::string to_string(const CharArray& array);
 
CharArray substring(const CharArray& array, std::size_t position,
                   std::size_t length = ArrayView::npos);
                   
}  // namespace Text

// ============================================================================
// 3. Matrix<T> — tabela 2D em ordem de linha (row-major)
// ============================================================================

template <typename T>
class Matrix {
public:
    Matrix() = default;

    Matrix(std::size_t rows, std::size_t cols, const T& value = T());
    

    // Redefine dimensões e preenche todas as células com 'value'.
    void assign(std::size_t rows, std::size_t cols, const T& value = T());

    void fill(const T& value);
    
    std::size_t rows() const noexcept;
    std::size_t cols() const noexcept;
    std::size_t size() const noexcept;
    bool empty() const noexcept;

    // Sem verificação de limites.
    T& operator()(std::size_t row, std::size_t col) noexcept;
  
    const T& operator()(std::size_t row, std::size_t col) const noexcept;
    
    T& at(std::size_t row, std::size_t col);
    
    const T& at(std::size_t row, std::size_t col) const;

private:
    DynamicArray<T> cells_;
    std::size_t rows_ = 0;
    std::size_t cols_ = 0;

    void check_bounds(std::size_t row, std::size_t col) const;
};

// ============================================================================
// 4. Armazenamento de resultados
// ============================================================================

using IndexArray    = DynamicArray<std::size_t>;  // posições de ocorrências
using DistanceArray = DynamicArray<std::size_t>;  // distâncias de edição

struct Match {
    std::size_t position;  // índice inicial da ocorrência no texto
    std::size_t length;    // tamanho do trecho encontrado
};
using MatchArray = DynamicArray<Match>;

// ============================================================================
// 5. Tabelas auxiliares dos algoritmos de busca e distância
// ============================================================================

// KMP: failure[i] = tamanho do maior prefixo próprio de pattern[0..i] que
// também é sufixo (função de prefixo / LPS).
struct KMPTable {
    IndexArray failure;
};

// Boyer-Moore:
//   bad_character     -> última posição de cada byte no padrão (-1 se ausente)
//   good_suffix_shift -> deslocamento da regra do bom sufixo, tamanho m + 1
//   suffix_border     -> bordas usadas no pré-processamento do bom sufixo
struct BoyerMooreTables {
    static constexpr std::size_t kAlphabetSize = 256;

    DynamicArray<std::ptrdiff_t> bad_character;
    IndexArray good_suffix_shift;
    IndexArray suffix_border;

    // Indexa pelo valor sem sinal: char pode ser negativo em algumas plataformas.
    static std::size_t index_of(char symbol) noexcept;
};

// Rabin-Karp: parâmetros do hash rolante e o hash de cada janela do texto.
struct RabinKarpTable {
    std::uint64_t base = 256;
    std::uint64_t modulus = 1'000'000'007ULL;
    std::uint64_t pattern_hash = 0;
    std::uint64_t high_order_factor = 1;  // base^(m-1) mod modulus
    DynamicArray<std::uint64_t> window_hashes;
};

// Levenshtein: matriz (|a|+1) x (|b|+1) de custos.
using LevenshteinMatrix = Matrix<std::size_t>;

// Factories: apenas alocam e inicializam o armazenamento; o preenchimento
// com os valores do algoritmo fica a cargo de quem o implementa.

namespace Tables {

KMPTable make_kmp_table(std::size_t pattern_length);

BoyerMooreTables make_boyer_moore_tables(std::size_t pattern_length);

RabinKarpTable make_rabin_karp_table(std::size_t text_length, std::size_t pattern_length,
                                     std::uint64_t base = 256,
                                     std::uint64_t modulus = 1'000'000'007ULL);
                                            
LevenshteinMatrix make_levenshtein_matrix(std::size_t length_a, std::size_t length_b);
 
}  // namespace Tables

}  // namespace DynamicArray

#endif  // DYNAMIC_ARRAY
