/**
 * StringBuffer: buffer de caracteres mutável com capacidade gerenciada.
 *
 * Invariantes:
 *  - size() <= capacity() em todos os momentos;
 *  - o conteúdo é sempre terminado em '\0' (c_str() é seguro), mas o '\0'
 *    final não conta em size() nem em capacity();
 *  - clear() e reduções de tamanho NUNCA diminuem a capacidade; apenas
 *    shrinkToFit() faz isso, de forma explícita.
 *
 * Garantia de exceção: operações que lançam exceção (std::out_of_range,
 * std::length_error, std::invalid_argument, std::bad_alloc) não alteram o
 * conteúdo do buffer.
 */
 
#ifndef STRING_BUFFER
#define STRING_BUFFER

#include <cstddef>
#include <memory>
#include <string>

#include "StringView.hpp"

using BufferView = StringView::StringView;

namespace StringBuffer {
    
class StringBuffer {
public:
    static constexpr std::size_t kDefaultCapacity = 16;
    static constexpr std::size_t npos = static_cast<std::size_t>(-1);

    // --- Construção, cópia, movimentação e destruição ---------------------
    StringBuffer();
    explicit StringBuffer(std::size_t initialCapacity);
    StringBuffer(BufferView initialContent);
    StringBuffer(const StringBuffer& other);
    StringBuffer(StringBuffer&& other) noexcept;
    StringBuffer& operator=(const StringBuffer& other);
    StringBuffer& operator=(StringBuffer&& other) noexcept;
    ~StringBuffer() = default;

    void swap(StringBuffer& other) noexcept;

    // --- Inserção ---------------------------------------------------------
    StringBuffer& insert(std::size_t pos, char ch);
    StringBuffer& insert(std::size_t pos, std::size_t count, char ch);
    StringBuffer& insert(std::size_t pos, BufferView text);

    StringBuffer& append(BufferView text);
    StringBuffer& append(std::size_t count, char ch);
    void pushBack(char ch);

    // --- Remoção ----------------------------------------------------------
    /// Remove até `count` caracteres a partir de `pos` (count é limitado ao
    /// que existe após pos; use npos para remover até o final).
    StringBuffer& erase(std::size_t pos, std::size_t count = npos);

    /// Esvazia o conteúdo preservando a capacidade alocada.
    void clear() noexcept;

    // --- Substituição -----------------------------------------------------
    /// Substitui até `count` caracteres a partir de `pos` por `text`.
    StringBuffer& replace(std::size_t pos, std::size_t count, BufferView text);

    /// Substitui todas as ocorrências não sobrepostas de `target` por
    /// `replacement`. Retorna o número de substituições feitas.
    std::size_t replaceAll(BufferView target, BufferView replacement);

    // --- Capacidade e tamanho --------------------------------------------
    /// Garante capacidade mínima de `newCapacity`. Nunca diminui a capacidade.
    void reserve(std::size_t newCapacity);

    /// Ajusta o tamanho. Ao crescer, preenche com `fill`; ao reduzir,
    /// trunca o conteúdo sem liberar memória.
    void resize(std::size_t newSize, char fill = '\0');

    /// Reduz a capacidade para exatamente size().
    void shrinkToFit();

    // --- Acesso e consulta -----------------------------------------------
    std::size_t size() const noexcept;
    std::size_t capacity() const noexcept;
    bool empty() const noexcept;

    char& operator[](std::size_t index) noexcept;
    char operator[](std::size_t index) const noexcept;
    char& at(std::size_t index);
    char at(std::size_t index) const;

    const char* c_str() const noexcept;
    BufferView view() const noexcept;
    std::string str() const;

private:
    /// Retorna string vazia válida mesmo para objetos "moved-from".
    const char* rawData_() const noexcept;

    void checkPosition(std::size_t pos, const char* operation) const;
    bool overlapsBuffer(BufferView text) const noexcept;
    void ensureCapacity(std::size_t required);
    void reallocate(std::size_t newCapacity);
    void terminate() noexcept;

    /// Remove `removeCount` caracteres em `pos` e abre espaço para
    /// `insertCount` novos, ajustando size_. Os bytes abertos ficam sem
    /// inicializar e devem ser preenchidos pelo chamador.
    void splice(std::size_t pos, std::size_t removeCount, std::size_t insertCount);

    std::unique_ptr<char[]> data_;
    std::size_t size_ = 0;
    std::size_t capacity_ = 0;
};

} // namespace StringBuffer

#endif // STRING_BUFFER
