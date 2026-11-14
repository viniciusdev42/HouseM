#include "DynamicArray.hpp"

using DV = ArrayView;

// ============================================================================
// 1. DynamicArray<T>
// ============================================================================

// --- Construção, cópia e movimentação --------------------------------------

template <typename T>
DynamicArray<T>::DynamicArray(size_type count) : DynamicArray() {
    resize(count);
}

template <typename T>
DynamicArray<T>::DynamicArray(size_type count, const T& value) : DynamicArray() {
    resize(count, value);
}

template <typename T>
DynamicArray<T>::DynamicArray(std::initializer_list<T> values) : DynamicArray() {
    reserve(values.size());
    for (const T& element : values) emplace_back(element);
}

template <typename T>
DynamicArray<T>::DynamicArray(const DynamicArray& other) : DynamicArray() {
    reserve(other.size_);
    for (const T& element : other) emplace_back(element);
}

template <typename T>
DynamicArray<T>::DynamicArray(DynamicArray&& other) noexcept
    : data_(std::exchange(other.data_, nullptr)),
      size_(std::exchange(other.size_, 0)),
      capacity_(std::exchange(other.capacity_, 0)) {}

template <typename T>
DynamicArray<T>& DynamicArray<T>::operator=(const DynamicArray& other) {
    DynamicArray copy(other);
    swap(copy);
    return *this;
}

template <typename T>
DynamicArray<T>& DynamicArray<T>::operator=(DynamicArray&& other) noexcept {
    DynamicArray moved(std::move(other));
    swap(moved);
    return *this;
}

template <typename T>
DynamicArray<T>::~DynamicArray() {
    destroy_range(data_, size_);
    deallocate(data_, capacity_);
}

// --- Capacidade -------------------------------------------------------------

template <typename T>
typename DynamicArray<T>::size_type DynamicArray<T>::size() const noexcept {
    return size_;
}

template <typename T>
typename DynamicArray<T>::size_type DynamicArray<T>::capacity() const noexcept {
    return capacity_;
}

template <typename T>
bool DynamicArray<T>::empty() const noexcept {
    return size_ == 0;
}

template <typename T>
typename DynamicArray<T>::size_type DynamicArray<T>::max_size() noexcept {
    return std::numeric_limits<size_type>::max() / sizeof(T);
}

template <typename T>
void DynamicArray<T>::reserve(size_type new_capacity) {
    if (new_capacity <= capacity_) return;
    if (new_capacity > max_size()) {
        throw std::length_error("DynamicArray::reserve: capacidade acima do maximo");
    }
    reallocate(new_capacity);
}

template <typename T>
void DynamicArray<T>::shrink_to_fit() {
    if (size_ == capacity_) return;
    if (size_ == 0) {
        deallocate(data_, capacity_);
        data_ = nullptr;
        capacity_ = 0;
        return;
    }
    reallocate(size_);
}

template <typename T>
void DynamicArray<T>::resize(size_type count) {
    resize_with(count, [](pointer slot) { ::new (static_cast<void*>(slot)) T(); });
}

template <typename T>
void DynamicArray<T>::resize(size_type count, const T& value) {
    // Cópia local: 'value' pode referenciar um elemento deste próprio array,
    // que ficaria inválido caso ocorra realocação.
    const T copy(value);
    resize_with(count, [&copy](pointer slot) {
        ::new (static_cast<void*>(slot)) T(copy);
    });
}

// --- Acesso -----------------------------------------------------------------

template <typename T>
typename DynamicArray<T>::reference DynamicArray<T>::operator[](size_type index) noexcept {
    return data_[index];
}

template <typename T>
typename DynamicArray<T>::const_reference DynamicArray<T>::operator[](size_type index) const noexcept {
    return data_[index];
}

template <typename T>
typename DynamicArray<T>::reference DynamicArray<T>::at(size_type index) {
    check_index(index);
    return data_[index];
}

template <typename T>
typename DynamicArray<T>::const_reference DynamicArray<T>::at(size_type index) const {
    check_index(index);
    return data_[index];
}

template <typename T>
typename DynamicArray<T>::reference DynamicArray<T>::front() {
    return at(0);
}

template <typename T>
typename DynamicArray<T>::const_reference DynamicArray<T>::front() const {
    return at(0);
}

template <typename T>
typename DynamicArray<T>::reference DynamicArray<T>::back() {
    return at(size_ == 0 ? 0 : size_ - 1);
}

template <typename T>
typename DynamicArray<T>::const_reference DynamicArray<T>::back() const {
    return at(size_ == 0 ? 0 : size_ - 1);
}

template <typename T>
typename DynamicArray<T>::pointer DynamicArray<T>::data() noexcept {
    return data_;
}

template <typename T>
typename DynamicArray<T>::const_pointer DynamicArray<T>::data() const noexcept {
    return data_;
}

template <typename T>
typename DynamicArray<T>::iterator DynamicArray<T>::begin() noexcept {
    return data_;
}

template <typename T>
typename DynamicArray<T>::iterator DynamicArray<T>::end() noexcept {
    return data_ + size_;
}

template <typename T>
typename DynamicArray<T>::const_iterator DynamicArray<T>::begin() const noexcept {
    return data_;
}

template <typename T>
typename DynamicArray<T>::const_iterator DynamicArray<T>::end() const noexcept {
    return data_ + size_;
}

template <typename T>
typename DynamicArray<T>::const_iterator DynamicArray<T>::cbegin() const noexcept {
    return begin();
}

template <typename T>
typename DynamicArray<T>::const_iterator DynamicArray<T>::cend() const noexcept {
    return end();
}

// --- Modificadores ----------------------------------------------------------

template <typename T>
template <typename... Args>
typename DynamicArray<T>::reference DynamicArray<T>::emplace_back(Args&&... args) {
    if (size_ == capacity_) {
        // Constrói antes de realocar: os argumentos podem referenciar
        // elementos do próprio array (ex.: push_back(arr[0])).
        T temporary(std::forward<Args>(args)...);
        reallocate(grown_capacity(size_ + 1));
        ::new (static_cast<void*>(data_ + size_)) T(std::move(temporary));
    } else {
        ::new (static_cast<void*>(data_ + size_)) T(std::forward<Args>(args)...);
    }
    ++size_;
    return data_[size_ - 1];
}

template <typename T>
void DynamicArray<T>::push_back(const T& value) {
    emplace_back(value);
}

template <typename T>
void DynamicArray<T>::push_back(T&& value) {
    emplace_back(std::move(value));
}

template <typename T>
void DynamicArray<T>::pop_back() {
    if (size_ == 0) throw std::out_of_range("DynamicArray::pop_back: array vazio");
    --size_;
    destroy_range(data_ + size_, 1);
}

template <typename T>
void DynamicArray<T>::insert(size_type index, const T& value) {
    if (index > size_) {
        throw std::out_of_range("DynamicArray::insert: indice " + std::to_string(index) +
                                " fora do intervalo [0, " + std::to_string(size_) + "]");
    }
    emplace_back(value);
    std::rotate(data_ + index, data_ + size_ - 1, data_ + size_);
}

template <typename T>
void DynamicArray<T>::erase(size_type index) {
    check_index(index);
    std::move(data_ + index + 1, data_ + size_, data_ + index);
    pop_back();
}

template <typename T>
void DynamicArray<T>::append(const T* source, size_type count) {
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
    if (inside_self) source = data_ + offset;  // buffer pode ter mudado

    for (size_type i = 0; i < count; ++i) {
        ::new (static_cast<void*>(data_ + size_)) T(source[i]);
        ++size_;
    }
}

template <typename T>
void DynamicArray<T>::append(const DynamicArray& other) {
    append(other.data_, other.size_);
}

template <typename T>
void DynamicArray<T>::clear() noexcept {
    destroy_range(data_, size_);
    size_ = 0;
}

template <typename T>
void DynamicArray<T>::swap(DynamicArray& other) noexcept {
    std::swap(data_, other.data_);
    std::swap(size_, other.size_);
    std::swap(capacity_, other.capacity_);
}

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

// --- Auxiliares privados ----------------------------------------------------

template <typename T>
typename DynamicArray<T>::pointer DynamicArray<T>::allocate(size_type count) {
    return count == 0 ? nullptr : std::allocator<T>().allocate(count);
}

template <typename T>
void DynamicArray<T>::deallocate(pointer block, size_type count) noexcept {
    if (block != nullptr) std::allocator<T>().deallocate(block, count);
}

template <typename T>
void DynamicArray<T>::destroy_range(pointer first, size_type count) noexcept {
    if constexpr (!std::is_trivially_destructible_v<T>) {
        for (size_type i = 0; i < count; ++i) first[i].~T();
    }
}

template <typename T>
void DynamicArray<T>::check_index(size_type index) const {
    if (index >= size_) {
        throw std::out_of_range("DynamicArray: indice " + std::to_string(index) +
                                " fora do intervalo (size = " + std::to_string(size_) + ")");
    }
}

template <typename T>
typename DynamicArray<T>::size_type DynamicArray<T>::grown_capacity(size_type min_required) const {
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
    return std::max(candidate, min_required);
}

template <typename T>
void DynamicArray<T>::reallocate(size_type new_capacity) {
    pointer new_block = allocate(new_capacity);
    size_type constructed = 0;
    try {
        for (; constructed < size_; ++constructed) {
            ::new (static_cast<void*>(new_block + constructed))
                T(std::move_if_noexcept(data_[constructed]));
        }
    } catch (...) {
        destroy_range(new_block, constructed);
        deallocate(new_block, new_capacity);
        throw;
    }
    destroy_range(data_, size_);
    deallocate(data_, capacity_);
    data_ = new_block;
    capacity_ = new_capacity;
}

template <typename T>
template <typename Construct>
void DynamicArray<T>::resize_with(size_type count, Construct construct) {
    if (count <= size_) {
        destroy_range(data_ + count, size_ - count);
        size_ = count;
        return;
    }
    if (count > capacity_) reallocate(grown_capacity(count));
    while (size_ < count) {
        construct(data_ + size_);
        ++size_;  // incrementa a cada elemento: estado consistente se lançar
    }
}

// ============================================================================
// 2. Sequências de caracteres
// ============================================================================

namespace Text {

CharArray make_char_array(DV content) {
    CharArray result;
    result.append(content.data(), content.size());
    return result;
}

void append_text(CharArray& target,DV content) {
    target.append(content.data(), content.size());
}

DV as_view(const CharArray& array) noexcept {
    return DV(array.data(), array.size());
}

std::string to_string(const CharArray& array) {
    return std::string(array.data(), array.size());
}

CharArray substring(const CharArray& array, std::size_t position, std::size_t length) {
    if (position > array.size()) {
        throw std::out_of_range("text::substring: posicao " + std::to_string(position) +
                                " maior que o tamanho " + std::to_string(array.size()));
    }
    const std::size_t effective = std::min(length, array.size() - position);
    CharArray result;
    result.append(array.data() + position, effective);
    return result;
}

}  // namespace Text

// ============================================================================
// 3. Matrix<T>
// ============================================================================

template <typename T>
Matrix<T>::Matrix(std::size_t rows, std::size_t cols, const T& value) {
    assign(rows, cols, value);
}

template <typename T>
void Matrix<T>::assign(std::size_t rows, std::size_t cols, const T& value) {
    if (cols != 0 && rows > std::numeric_limits<std::size_t>::max() / cols) {
        throw std::length_error("Matrix::assign: dimensoes excedem o limite");
    }
    rows_ = 0;
    cols_ = 0;
    cells_.clear();
    cells_.resize(rows * cols, value);
    rows_ = rows;
    cols_ = cols;
}

template <typename T>
void Matrix<T>::fill(const T& value) {
    std::fill(cells_.begin(), cells_.end(), value);
}

template <typename T>
std::size_t Matrix<T>::rows() const noexcept {
    return rows_;
}

template <typename T>
std::size_t Matrix<T>::cols() const noexcept {
    return cols_;
}

template <typename T>
std::size_t Matrix<T>::size() const noexcept {
    return cells_.size();
}

template <typename T>
bool Matrix<T>::empty() const noexcept {
    return cells_.empty();
}

template <typename T>
T& Matrix<T>::operator()(std::size_t row, std::size_t col) noexcept {
    return cells_[row * cols_ + col];
}

template <typename T>
const T& Matrix<T>::operator()(std::size_t row, std::size_t col) const noexcept {
    return cells_[row * cols_ + col];
}

template <typename T>
T& Matrix<T>::at(std::size_t row, std::size_t col) {
    check_bounds(row, col);
    return cells_[row * cols_ + col];
}

template <typename T>
const T& Matrix<T>::at(std::size_t row, std::size_t col) const {
    check_bounds(row, col);
    return cells_[row * cols_ + col];
}

template <typename T>
void Matrix<T>::check_bounds(std::size_t row, std::size_t col) const {
    if (row >= rows_ || col >= cols_) {
        throw std::out_of_range("Matrix: posicao (" + std::to_string(row) + ", " +
                                std::to_string(col) + ") fora de " + std::to_string(rows_) +
                                "x" + std::to_string(cols_));
    }
}

// ============================================================================
// 5. Tabelas auxiliares
// ============================================================================

std::size_t BoyerMooreTables::index_of(char symbol) noexcept {
    return static_cast<unsigned char>(symbol);
}

namespace Tables {

KmpTable make_kmp_table(std::size_t pattern_length) {
    KmpTable table;
    table.failure.resize(pattern_length, 0);
    return table;
}

BoyerMooreTables make_boyer_moore_tables(std::size_t pattern_length) {
    BoyerMooreTables tables;
    tables.bad_character.resize(BoyerMooreTables::kAlphabetSize, -1);
    tables.good_suffix_shift.resize(pattern_length + 1, 0);
    tables.suffix_border.resize(pattern_length + 1, 0);
    return tables;
}

RabinKarpTable make_rabin_karp_table(std::size_t text_length, std::size_t pattern_length,
                                     std::uint64_t base, std::uint64_t modulus) {
    if (modulus == 0) {
        throw std::invalid_argument("make_rabin_karp_table: modulo nao pode ser zero");
    }
    RabinKarpTable table;
    table.base = base;
    table.modulus = modulus;
    const std::size_t windows = text_length >= pattern_length ? text_length - pattern_length + 1 : 0;
    table.window_hashes.resize(windows, 0);
    return table;
}

LevenshteinMatrix make_levenshtein_matrix(std::size_t length_a, std::size_t length_b) {
    constexpr std::size_t kMax = std::numeric_limits<std::size_t>::max();
    if (length_a == kMax || length_b == kMax) {
        throw std::length_error("make_levenshtein_matrix: tamanho excede o limite");
    }
    return LevenshteinMatrix(length_a + 1, length_b + 1, 0);
}

}  // namespace Tables

// ============================================================================
// Instanciações explícitas (os templates são definidos apenas neste .cpp)
// ============================================================================

namespace {

// Em plataformas onde uint64_t == size_t, a instanciação seria duplicada;
// nesse caso usa-se um tipo sentinela distinto.
struct UnusedPlaceholder {
    bool operator==(const UnusedPlaceholder&) const noexcept { return true; }
};

using HashValue = std::conditional_t<std::is_same_v<std::uint64_t, std::size_t>,
                                     UnusedPlaceholder, std::uint64_t>;

}  // namespace

#define INSTANTIATE_DYNAMIC_ARRAY_MEMBERS(Type)                                          \
    template class DynamicArray<Type>;                                                   \
    template Type& DynamicArray<Type>::emplace_back<const Type&>(const Type&);           \
    template Type& DynamicArray<Type>::emplace_back<Type>(Type&&);

#define INSTANTIATE_DYNAMIC_ARRAY_COMPARABLE(Type)                                       \
    INSTANTIATE_DYNAMIC_ARRAY_MEMBERS(Type)                                              \
    template void swap(DynamicArray<Type>&, DynamicArray<Type>&) noexcept;               \
    template bool operator==(const DynamicArray<Type>&, const DynamicArray<Type>&);      \
    template bool operator!=(const DynamicArray<Type>&, const DynamicArray<Type>&);

INSTANTIATE_DYNAMIC_ARRAY_COMPARABLE(char)
INSTANTIATE_DYNAMIC_ARRAY_COMPARABLE(std::size_t)
INSTANTIATE_DYNAMIC_ARRAY_COMPARABLE(std::ptrdiff_t)
INSTANTIATE_DYNAMIC_ARRAY_COMPARABLE(HashValue)

// Match não define operator==, então só os membros e o swap são instanciados.
INSTANTIATE_DYNAMIC_ARRAY_MEMBERS(Match)
template void swap(DynamicArray<Match>&, DynamicArray<Match>&) noexcept;

template class Matrix<std::size_t>;

#undef INSTANTIATE_DYNAMIC_ARRAY_MEMBERS
#undef INSTANTIATE_DYNAMIC_ARRAY_COMPARABLE
