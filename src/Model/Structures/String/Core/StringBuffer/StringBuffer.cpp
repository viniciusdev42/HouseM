#include "StringBuffer.hpp"

#include <algorithm>
#include <cstring>
#include <functional>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace StringBuffer {

namespace {

// Limite conservador para evitar overflow em contas de capacidade + 1 e
// em somas de tamanhos.
constexpr std::size_t kMaxSize = std::numeric_limits<std::size_t>::max() / 2 - 1;
constexpr std::size_t kNpos = std::numeric_limits<std::size_t>::max();

void ensureWithinMaxSize(std::size_t requested, const char* operation) {
    if (requested > kMaxSize) {
        throw std::length_error(std::string("StringBuffer::") + operation +
                                ": tamanho solicitado excede o maximo suportado");
    }
}

} // namespace

// Cópia estável de uma view(), independente do buffer de origem.
std::string toStdString(BV text) {
    return text.empty() ? std::string() : std::string(text.data(), text.size());
}

BV toView(const std::string& text) {
    return BV(text.data(), text.size());
}

// Busca `needle` em `haystack` a partir de `from`, usando substr() e
// operator== de StringView. Retorna kNpos se não encontrar.
std::size_t findFrom(BV haystack, BV needle, std::size_t from) {
    if (needle.size() > haystack.size()) {
        return kNpos;
    }
    const std::size_t lastStart = haystack.size() - needle.size();
    for (std::size_t i = from; i <= lastStart; ++i) {
        if (haystack.substr(i, needle.size()) == needle) {
            return i;
        }
    }
    return kNpos;
}

std::size_t StringBuffer::size() const noexcept { return size_; }

std::size_t StringBuffer::capacity() const noexcept { return capacity_; }

bool StringBuffer::empty() const noexcept { return size_ == 0; }

char& StringBuffer::operator[](std::size_t i) noexcept { return data_[i]; }

char StringBuffer::operator[](std::size_t i) const noexcept { return data_[i]; }

const char* StringBuffer::c_str() const noexcept { return data_ ? data_.get() : ""; }

BufferView StringBuffer::view() const noexcept { return BufferView(c_str(), size_); }

std::string StringBuffer::str() const { return std::string(c_str(), size_); }

// ---------------------------------------------------------------------------
// Construção, cópia, movimentação
// ---------------------------------------------------------------------------

StringBuffer::StringBuffer() : StringBuffer(kDefaultCapacity) {}

StringBuffer::StringBuffer(std::size_t initialCapacity) {
    ensureWithinMaxSize(initialCapacity, "StringBuffer");
    // +1 reservado para o terminador '\0'.
    data_.reset(new char[initialCapacity + 1]);
    capacity_ = initialCapacity;
    terminate();
}

StringBuffer::StringBuffer(BufferView initialContent)
    : StringBuffer(std::max(static_cast<std::size_t>(initialContent.size()), kDefaultCapacity)) {
    append(initialContent);
}

StringBuffer::StringBuffer(const StringBuffer& other)
    : data_(new char[other.capacity_ + 1]),
      size_(other.size_),
      capacity_(other.capacity_) {
    // A capacidade da origem é preservada na cópia.
    if (size_ > 0) {
        std::memcpy(data_.get(), other.data_.get(), size_);
    }
    terminate();
}

StringBuffer::StringBuffer(StringBuffer&& other) noexcept
    : data_(std::move(other.data_)),
      size_(other.size_),
      capacity_(other.capacity_) {
    // O objeto movido fica vazio e válido (capacidade 0, c_str() == "").
    other.size_ = 0;
    other.capacity_ = 0;
}

StringBuffer& StringBuffer::operator=(const StringBuffer& other) {
    if (this != &other) {
        // Copy-and-swap: se a alocação falhar, *this permanece intacto.
        StringBuffer copy(other);
        swap(copy);
    }
    return *this;
}

StringBuffer& StringBuffer::operator=(StringBuffer&& other) noexcept {
    if (this != &other) {
        data_ = std::move(other.data_);
        size_ = other.size_;
        capacity_ = other.capacity_;
        other.size_ = 0;
        other.capacity_ = 0;
    }
    return *this;
}

void StringBuffer::swap(StringBuffer& other) noexcept {
    data_.swap(other.data_);
    std::swap(size_, other.size_);
    std::swap(capacity_, other.capacity_);
}

// ---------------------------------------------------------------------------
// Inserção
// ---------------------------------------------------------------------------

StringBuffer& StringBuffer::insert(std::size_t pos, char ch) {
    return insert(pos, 1, ch);
}

StringBuffer& StringBuffer::insert(std::size_t pos, std::size_t count, char ch) {
    checkPosition(pos, "insert");
    if (count == 0) {
        return *this;
    }
    splice(pos, 0, count);
    std::memset(data_.get() + pos, static_cast<unsigned char>(ch), count);
    return *this;
}

StringBuffer& StringBuffer::insert(std::size_t pos, BufferView text) {
    checkPosition(pos, "insert");
    if (text.empty()) {
        return *this;
    }
    // Se `text` aponta para dentro do próprio buffer, uma realocação ou o
    // deslocamento da cauda invalidaria a origem; copiamos antes.
    if (overlapsBuffer(text)) {
        const std::string stableCopy = toStdString(text);
        return insert(pos, toView(stableCopy));
    }
    splice(pos, 0, text.size());
    std::memcpy(data_.get() + pos, text.data(), text.size());
    return *this;
}

StringBuffer& StringBuffer::append(BufferView text) {
    return insert(size_, text);
}

StringBuffer& StringBuffer::append(std::size_t count, char ch) {
    return insert(size_, count, ch);
}

void StringBuffer::pushBack(char ch) {
    insert(size_, 1, ch);
}

// ---------------------------------------------------------------------------
// Remoção
// ---------------------------------------------------------------------------

StringBuffer& StringBuffer::erase(std::size_t pos, std::size_t count) {
    checkPosition(pos, "erase");
    const std::size_t effectiveCount = std::min(count, size_ - pos);
    splice(pos, effectiveCount, 0);
    return *this;
}

void StringBuffer::clear() noexcept {
    size_ = 0;
    terminate();
}

// ---------------------------------------------------------------------------
// Substituição
// ---------------------------------------------------------------------------

StringBuffer& StringBuffer::replace(std::size_t pos, std::size_t count,
                                    BufferView text) {
    checkPosition(pos, "replace");
    const std::size_t effectiveCount = std::min(count, size_ - pos);
    if (effectiveCount == 0 && text.empty()) {
        return *this;
    }
    if (overlapsBuffer(text)) {
        const std::string stableCopy = toStdString(text);
        return replace(pos, count, toView(stableCopy));
    }
    splice(pos, effectiveCount, text.size());
    if (!text.empty()) {
        std::memcpy(data_.get() + pos, text.data(), text.size());
    }
    return *this;
}

std::size_t StringBuffer::replaceAll(BufferView target,
                                     BufferView replacement) {
    
 if (target.empty()) {
        throw std::invalid_argument("StringBuffer::replaceAll: o trecho a localizar nao pode ser vazio");
    }
    const BufferView src = view();
    std::string out;
    std::size_t count = 0, from = 0, idx;
    while ((idx = src.find(target, from)) != BufferView::npos) {
        out.append(src.data() + from, idx - from);
        out.append(replacement.data(), replacement.size());
        from = idx + target.size();
        ++count;
    }
    if (count == 0) return 0;
    out.append(src.data() + from, src.size() - from);

    ensureWithinMaxSize(out.size(), "replaceAll");
    ensureCapacity(out.size());              // pode lançar: nada foi alterado ainda
    std::memcpy(data_.get(), out.data(), out.size());
    size_ = out.size();
    terminate();
    return count;
   }

// ---------------------------------------------------------------------------
// Capacidade e tamanho
// ---------------------------------------------------------------------------

void StringBuffer::reserve(std::size_t newCapacity) {
    if (newCapacity <= capacity_) {
        return;
    }
    ensureWithinMaxSize(newCapacity, "reserve");
    reallocate(newCapacity);
}

void StringBuffer::resize(std::size_t newSize, char fill) {
    if (newSize <= size_) {
        // Truncamento: a capacidade é preservada.
        size_ = newSize;
        terminate();
        return;
    }
    ensureWithinMaxSize(newSize, "resize");
    ensureCapacity(newSize);
    std::memset(data_.get() + size_, static_cast<unsigned char>(fill), newSize - size_);
    size_ = newSize;
    terminate();
}

void StringBuffer::shrinkToFit() {
    if (capacity_ != size_ || !data_) {
        reallocate(size_);
    }
}

// ---------------------------------------------------------------------------
// Acesso
// ---------------------------------------------------------------------------

char& StringBuffer::at(std::size_t index) {
    if (index >= size_) {
        throw std::out_of_range("StringBuffer::at: indice " + std::to_string(index) +
                                " fora do intervalo [0, " + std::to_string(size_) + ")");
    }
    return data_[index];
}

char StringBuffer::at(std::size_t index) const {
    if (index >= size_) {
        throw std::out_of_range("StringBuffer::at: indice " + std::to_string(index) +
                                " fora do intervalo [0, " + std::to_string(size_) + ")");
    }
    return data_[index];
}

// ---------------------------------------------------------------------------
// Auxiliares privados
// ---------------------------------------------------------------------------

void StringBuffer::checkPosition(std::size_t pos, const char* operation) const {
    if (pos > size_) {
        throw std::out_of_range(std::string("StringBuffer::") + operation + ": posicao " +
                                std::to_string(pos) + " fora do intervalo [0, " +
                                std::to_string(size_) + "]");
    }
}

bool StringBuffer::overlapsBuffer(BufferView text) const noexcept {
    if (!data_ || text.empty()) {
        return false;
    }
    const std::less<const char*> before;
    const char* bufferBegin = data_.get();
    const char* bufferEnd = bufferBegin + size_ + 1;  // inclui o '\0'
    return before(text.data(), bufferEnd) && before(bufferBegin, text.data() + text.size());
}

void StringBuffer::ensureCapacity(std::size_t required) {
    if (required <= capacity_) {
        return;
    }
    ensureWithinMaxSize(required, "ensureCapacity");
    // Crescimento geométrico mantém o custo amortizado de inserções O(1).
    const std::size_t grown = capacity_ > kMaxSize / 2 ? kMaxSize : capacity_ * 2;
    reallocate(std::max(required, grown));
}

void StringBuffer::reallocate(std::size_t newCapacity) {
    // Aloca antes de modificar qualquer estado: se lançar, nada muda.
    std::unique_ptr<char[]> fresh(new char[newCapacity + 1]);
    if (size_ > 0) {
        std::memcpy(fresh.get(), data_.get(), size_);
    }
    data_ = std::move(fresh);
    capacity_ = newCapacity;
    terminate();
}

void StringBuffer::terminate() noexcept {
    if (data_) {
        data_[size_] = '\0';
    }
}

void StringBuffer::splice(std::size_t pos, std::size_t removeCount, std::size_t insertCount) {
    if (removeCount == 0 && insertCount == 0) {
        return;
    }
    if (insertCount > removeCount && insertCount - removeCount > kMaxSize - size_) {
        throw std::length_error("StringBuffer: tamanho resultante excede o maximo suportado");
    }
    const std::size_t newSize = size_ - removeCount + insertCount;
    ensureCapacity(newSize);

    const std::size_t tailLength = size_ - pos - removeCount;
    if (tailLength > 0 && insertCount != removeCount) {
        std::memmove(data_.get() + pos + insertCount,
                     data_.get() + pos + removeCount,
                     tailLength);
    }
    size_ = newSize;
    terminate();
}

} // namespace StringBuffer