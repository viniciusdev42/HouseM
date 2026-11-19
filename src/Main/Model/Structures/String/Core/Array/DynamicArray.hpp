#ifndef DYNAMIC_ARRAY_HPP
#define DYNAMIC_ARRAY_HPP

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

#include "src/Model/Structures/String/Core/View/StringView.hpp"

namespace DynamicArray {

using ArrayView = StringView::StringView;

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

    // Os construtores delegam ao padrão: se algo lançar no corpo, o objeto já
    // está construído e o destrutor libera a memória.
    DynamicArray() noexcept = default;

    explicit DynamicArray(size_type count) : DynamicArray() { resize(count); }

    DynamicArray(size_type count, const T& value) : DynamicArray() { resize(count, value); }

    DynamicArray(std::initializer_list<T> values) : DynamicArray() {
        reserve(values.size());
        append(values.begin(), values.size());
    }

    DynamicArray(const DynamicArray& other) : DynamicArray() {
        reserve(other.size_);
        append(other.data_, other.size_);
    }

    DynamicArray(DynamicArray&& other) noexcept
        : data_(std::exchange(other.data_, nullptr)),
          size_(std::exchange(other.size_, 0)),
          capacity_(std::exchange(other.capacity_, 0)) {}

    // Copy-and-swap: garantia forte de exceção e auto-atribuição segura.
    DynamicArray& operator=(const DynamicArray& other) {
        DynamicArray copy(other);
        swap(copy);
        return *this;
    }

    DynamicArray& operator=(DynamicArray&& other) noexcept {
        DynamicArray moved(std::move(other));
        swap(moved);
        return *this;
    }

    ~DynamicArray() {
        destroy_range(data_, size_);
        deallocate(data_, capacity_);
    }

    // --- Capacidade ---------------------------------------------------------

    size_type size() const noexcept { return size_; }
    size_type capacity() const noexcept { return capacity_; }
    bool empty() const noexcept { return size_ == 0; }

    // Limitado por PTRDIFF_MAX, que é o limite real do std::allocator.
    static size_type max_size() noexcept {
        return static_cast<size_type>(std::numeric_limits<std::ptrdiff_t>::max()) / sizeof(T);
    }

    // Capacidade exata solicitada; nunca diminui.
    void reserve(size_type new_capacity) {
        if (new_capacity <= capacity_) return;
        if (new_capacity > max_size()) {
            throw std::length_error("DynamicArray::reserve: capacidade acima do maximo");
        }
        reallocate(new_capacity);
    }

    void shrink_to_fit() {
        if (size_ == capacity_) return;
        if (size_ == 0) {
            deallocate(data_, capacity_);
            data_ = nullptr;
            capacity_ = 0;
            return;
        }
        reallocate(size_);
    }

    // Novos elementos são value-initialized.
    void resize(size_type count) {
        resize_with(count, [](pointer slot) { ::new (static_cast<void*>(slot)) T(); });
    }

    void resize(size_type count, const T& value) {
        // Cópia local: 'value' pode referenciar um elemento deste array,
        // que ficaria inválido se houvesse realocação.
        const T copy(value);
        resize_with(count, [&copy](pointer slot) { ::new (static_cast<void*>(slot)) T(copy); });
    }

    // --- Acesso -------------------------------------------------------------

    // Sem verificação de limites (como std::vector); use at() quando preciso.
    reference operator[](size_type index) noexcept { return data_[index]; }
    const_reference operator[](size_type index) const noexcept { return data_[index]; }

    reference at(size_type index) {
        check_index(index);
        return data_[index];
    }
    const_reference at(size_type index) const {
        check_index(index);
        return data_[index];
    }

    reference front() {
        require_non_empty();
        return data_[0];
    }
    const_reference front() const {
        require_non_empty();
        return data_[0];
    }
    reference back() {
        require_non_empty();
        return data_[size_ - 1];
    }
    const_reference back() const {
        require_non_empty();
        return data_[size_ - 1];
    }

    pointer data() noexcept { return data_; }
    const_pointer data() const noexcept { return data_; }

    iterator begin() noexcept { return data_; }
    iterator end() noexcept { return data_ + size_; }
    const_iterator begin() const noexcept { return data_; }
    const_iterator end() const noexcept { return data_ + size_; }
    const_iterator cbegin() const noexcept { return data_; }
    const_iterator cend() const noexcept { return data_ + size_; }

    // --- Modificadores ------------------------------------------------------

    // Na realocação, o novo elemento é construído no bloco novo ANTES de mover
    // os antigos: argumentos que referenciam o próprio array continuam válidos
    // e T não precisa ser movível.
    template <typename... Args>
    reference emplace_back(Args&&... args) {
        if (size_ == capacity_) {
            const size_type new_capacity = grown_capacity(size_ + 1);
            pointer block = allocate(new_capacity);
            try {
                ::new (static_cast<void*>(block + size_)) T(std::forward<Args>(args)...);
                try {
                    move_construct_range(block, data_, size_);
                } catch (...) {
                    destroy_range(block + size_, 1);
                    throw;
                }
            } catch (...) {
                deallocate(block, new_capacity);
                throw;
            }
            adopt(block, new_capacity);
        } else {
            ::new (static_cast<void*>(data_ + size_)) T(std::forward<Args>(args)...);
        }
        ++size_;
        return data_[size_ - 1];
    }

    void push_back(const T& value) { emplace_back(value); }
    void push_back(T&& value) { emplace_back(std::move(value)); }

    void pop_back() {
        if (size_ == 0) throw std::out_of_range("DynamicArray::pop_back: array vazio");
        --size_;
        destroy_range(data_ + size_, 1);
    }

    // Insere na posição 'index' (0 <= index <= size()).
    void insert(size_type index, const T& value) { insert_at(index, value); }
    void insert(size_type index, T&& value) { insert_at(index, std::move(value)); }

    void erase(size_type index) {
        check_index(index);
        std::move(data_ + index + 1, data_ + size_, data_ + index);
        pop_back();
    }

    // Aceita faixas do próprio array.
    void append(const T* source, size_type count) {
        if (count == 0) return;
        if (source == nullptr) {
            throw std::invalid_argument("DynamicArray::append: origem nula com count > 0");
        }
        if (count > max_size() - size_) {
            throw std::length_error("DynamicArray::append: tamanho acima do maximo");
        }

        const std::less<const T*> before;
        const bool inside_self = !before(source, data_) && before(source, data_ + size_);
        const size_type offset = inside_self ? static_cast<size_type>(source - data_) : 0;

        if (size_ + count > capacity_) reallocate(grown_capacity(size_ + count));
        if (inside_self) source = data_ + offset;  // o buffer pode ter mudado

        for (size_type i = 0; i < count; ++i) {
            ::new (static_cast<void*>(data_ + size_)) T(source[i]);
            ++size_;
        }
    }

    void append(const DynamicArray& other) { append(other.data_, other.size_); }

    // Destrói os elementos mas mantém a capacidade.
    void clear() noexcept {
        destroy_range(data_, size_);
        size_ = 0;
    }

    void swap(DynamicArray& other) noexcept {
        std::swap(data_, other.data_);
        std::swap(size_, other.size_);
        std::swap(capacity_, other.capacity_);
    }

private:
    pointer data_ = nullptr;
    size_type size_ = 0;
    size_type capacity_ = 0;

    static pointer allocate(size_type count) {
        if (count == 0) return nullptr;
        if (count > max_size()) {
            throw std::length_error("DynamicArray: alocacao acima do maximo");
        }
        return std::allocator<T>().allocate(count);
    }

    static void deallocate(pointer block, size_type count) noexcept {
        if (block != nullptr) std::allocator<T>().deallocate(block, count);
    }

    static void destroy_range(pointer first, size_type count) noexcept {
        if constexpr (!std::is_trivially_destructible_v<T>) {
            for (size_type i = 0; i < count; ++i) first[i].~T();
        }
    }

    // Se lançar, destrói o que já construiu em 'dest'; a origem fica intacta
    // quando T só tem cópia (move_if_noexcept).
    static void move_construct_range(pointer dest, pointer source, size_type count) {
        size_type done = 0;
        try {
            for (; done < count; ++done) {
                ::new (static_cast<void*>(dest + done)) T(std::move_if_noexcept(source[done]));
            }
        } catch (...) {
            destroy_range(dest, done);
            throw;
        }
    }

    // Troca o bloco atual por 'block', que já contém os elementos movidos.
    void adopt(pointer block, size_type new_capacity) noexcept {
        destroy_range(data_, size_);
        deallocate(data_, capacity_);
        data_ = block;
        capacity_ = new_capacity;
    }

    void check_index(size_type index) const {
        if (index >= size_) {
            throw std::out_of_range("DynamicArray: indice " + std::to_string(index) +
                                    " fora do intervalo (size = " + std::to_string(size_) + ")");
        }
    }

    void require_non_empty() const {
        if (size_ == 0) throw std::out_of_range("DynamicArray: array vazio");
    }

    // Crescimento geométrico (x2) => push_back com custo amortizado O(1).
    size_type grown_capacity(size_type min_required) const {
        if (min_required > max_size()) {
            throw std::length_error("DynamicArray: capacidade acima do maximo");
        }
        size_type candidate;
        if (capacity_ == 0) {
            candidate = kDefaultCapacity;
        } else if (capacity_ > max_size() / kGrowthFactor) {
            candidate = max_size();
        } else {
            candidate = capacity_ * kGrowthFactor;
        }
        candidate = std::min(candidate, max_size());
        return std::max(candidate, min_required);
    }

    // Mantém o array intacto se algo lançar.
    void reallocate(size_type new_capacity) {
        pointer block = allocate(new_capacity);
        try {
            move_construct_range(block, data_, size_);
        } catch (...) {
            deallocate(block, new_capacity);
            throw;
        }
        adopt(block, new_capacity);
    }

    template <typename Construct>
    void resize_with(size_type count, Construct construct) {
        if (count <= size_) {
            destroy_range(data_ + count, size_ - count);
            size_ = count;
            return;
        }
        if (count > capacity_) reallocate(grown_capacity(count));
        while (size_ < count) {
            construct(data_ + size_);
            ++size_;  // a cada elemento: estado consistente se lançar
        }
    }

    template <typename U>
    void insert_at(size_type index, U&& value) {
        if (index > size_) {
            throw std::out_of_range("DynamicArray::insert: indice " + std::to_string(index) +
                                    " fora do intervalo [0, " + std::to_string(size_) + "]");
        }
        emplace_back(std::forward<U>(value));
        std::rotate(data_ + index, data_ + size_ - 1, data_ + size_);
    }
};

template <typename T>
void swap(DynamicArray<T>& a, DynamicArray<T>& b) noexcept {
    a.swap(b);
}

template <typename T>
bool operator==(const DynamicArray<T>& a, const DynamicArray<T>& b) {
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin());
}

template <typename T>
bool operator!=(const DynamicArray<T>& a, const DynamicArray<T>& b) {
    return !(a == b);
}

// ============================================================================
// 2. Sequências de caracteres
// ============================================================================

using CharArray = DynamicArray<char>;

// Funções livres em vez de herança: DynamicArray não tem destrutor virtual.
namespace Text {

CharArray make_char_array(ArrayView content);

void append_text(CharArray& target, ArrayView content);

// A view não possui os dados: invalida se o array realocar ou for destruído.
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

    Matrix(std::size_t rows, std::size_t cols, const T& value = T()) {
        assign(rows, cols, value);
    }

    Matrix(const Matrix&) = default;
    Matrix& operator=(const Matrix&) = default;

    // Move explícito: a origem não pode ficar com dimensões sem células.
    Matrix(Matrix&& other) noexcept
        : cells_(std::move(other.cells_)),
          rows_(std::exchange(other.rows_, 0)),
          cols_(std::exchange(other.cols_, 0)) {}

    Matrix& operator=(Matrix&& other) noexcept {
        if (this != &other) {
            cells_ = std::move(other.cells_);
            rows_ = std::exchange(other.rows_, 0);
            cols_ = std::exchange(other.cols_, 0);
        }
        return *this;
    }

    ~Matrix() = default;

    // Redefine dimensões e preenche todas as células com 'value'.
    void assign(std::size_t rows, std::size_t cols, const T& value = T()) {
        if (cols != 0 && rows > std::numeric_limits<std::size_t>::max() / cols) {
            throw std::length_error("Matrix::assign: dimensoes excedem o limite");
        }
        // Cópia antes do clear(): 'value' pode ser uma célula desta matriz.
        const T copy(value);
        rows_ = 0;
        cols_ = 0;
        cells_.clear();
        cells_.resize(rows * cols, copy);
        rows_ = rows;
        cols_ = cols;
    }

    void fill(const T& value) { std::fill(cells_.begin(), cells_.end(), value); }

    std::size_t rows() const noexcept { return rows_; }
    std::size_t cols() const noexcept { return cols_; }
    std::size_t size() const noexcept { return cells_.size(); }
    bool empty() const noexcept { return cells_.empty(); }

    // Sem verificação de limites.
    T& operator()(std::size_t row, std::size_t col) noexcept { return cells_[row * cols_ + col]; }
    const T& operator()(std::size_t row, std::size_t col) const noexcept {
        return cells_[row * cols_ + col];
    }

    T& at(std::size_t row, std::size_t col) {
        check_bounds(row, col);
        return cells_[row * cols_ + col];
    }
    const T& at(std::size_t row, std::size_t col) const {
        check_bounds(row, col);
        return cells_[row * cols_ + col];
    }

private:
    DynamicArray<T> cells_;
    std::size_t rows_ = 0;
    std::size_t cols_ = 0;

    void check_bounds(std::size_t row, std::size_t col) const {
        if (row >= rows_ || col >= cols_) {
            throw std::out_of_range("Matrix: posicao (" + std::to_string(row) + ", " +
                                    std::to_string(col) + ") fora de " + std::to_string(rows_) +
                                    "x" + std::to_string(cols_));
        }
    }
};

// ============================================================================
// 4. Armazenamento de resultados
// ============================================================================

using IndexArray    = DynamicArray<std::size_t>;  // posições de ocorrências
using DistanceArray = DynamicArray<std::size_t>;  // distâncias de edição

struct Match {
    std::size_t position = 0;  // índice inicial da ocorrência no texto
    std::size_t length = 0;    // tamanho do trecho encontrado

    constexpr Match() noexcept = default;
    // Construtor explícito: permite emplace_back(position, length) em C++17.
    constexpr Match(std::size_t pos, std::size_t len) noexcept : position(pos), length(len) {}
};

constexpr bool operator==(const Match& a, const Match& b) noexcept {
    return a.position == b.position && a.length == b.length;
}
constexpr bool operator!=(const Match& a, const Match& b) noexcept { return !(a == b); }

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
    static std::size_t index_of(char symbol) noexcept {
        return static_cast<unsigned char>(symbol);
    }
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

namespace Tables {

KMPTable make_kmp_table(std::size_t pattern_length);

BoyerMooreTables make_boyer_moore_tables(std::size_t pattern_length);

// 'modulus' deve estar em [1, 2^32] para que produtos de dois resíduos caibam
// em 64 bits. 'base' é reduzido módulo 'modulus'; high_order_factor já sai
// calculado, pattern_hash e window_hashes ficam a cargo do algoritmo.
RabinKarpTable make_rabin_karp_table(std::size_t text_length, std::size_t pattern_length,
                                     std::uint64_t base = 256,
                                     std::uint64_t modulus = 1'000'000'007ULL);

LevenshteinMatrix make_levenshtein_matrix(std::size_t length_a, std::size_t length_b);

}  // namespace Tables

}  // namespace DynamicArray

#endif  // DYNAMIC_ARRAY_HPP
