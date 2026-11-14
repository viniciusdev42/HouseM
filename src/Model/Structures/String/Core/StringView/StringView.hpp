/**
 * Visão somente leitura e não proprietária sobre uma sequência contígua de
 * caracteres.
 *
 * Responsabilidade única: referenciar (ponteiro + tamanho) dados que
 * pertencem a outra entidade. A classe nunca aloca, libera nem modifica o
 * conteúdo referenciado.
 *
 * Contrato de tempo de vida: o chamador deve garantir que a memória
 * referenciada permaneça válida enquanto a view (e qualquer subview ou
 * iterador derivado dela) estiver em uso.
 *
 * A sequência NÃO precisa ser terminada em '\0'. Por isso, data() não deve
 * ser usado como C-string sem que isso seja garantido pelo chamador.
 */

#ifndef STRING_VIEW
#define STRING_VIEW

#include <cstddef>
#include <iterator>
#include <string>

namespace StringView {
    
class StringView {
public:
    using value_type             = char;
    using size_type              = std::size_t;
    using difference_type        = std::ptrdiff_t;
    using const_pointer          = const char*;
    using const_reference        = const char&;
    using const_iterator         = const char*;
    using iterator               = const_iterator;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    using reverse_iterator       = const_reverse_iterator;

    /// Valor especial: "até o fim da view" (usado em substr).
    static constexpr size_type npos = static_cast<size_type>(-1);

    // ------------------------------------------------------------------
    // Construção
    // ------------------------------------------------------------------

    /// View vazia.
    constexpr StringView() noexcept = default;

    /// View sobre o conteúdo de uma std::string (não estende seu tempo de vida).
    StringView(const std::string& str) noexcept;
    
    /// Primeira ocorrência de `needle` a partir de `pos`, ou npos.
    /// Uma `needle` vazia retorna pos (se pos <= size()).
    size_type find(StringView needle, size_type pos = 0) const noexcept;

    /// Primeira ocorrência de `ch` a partir de `pos`, ou npos.
    size_type find(char ch, size_type pos = 0) const noexcept;
    /// View sobre uma C-string terminada em '\0' (o terminador não faz
    /// parte da view). Um ponteiro nulo resulta em view vazia.
    StringView(const char* cString) noexcept;

    // ------------------------------------------------------------------
    // Capacidade
    // ------------------------------------------------------------------

    constexpr size_type size() const noexcept;
    constexpr size_type length() const noexcept;
    constexpr bool empty() const noexcept;

    // ------------------------------------------------------------------
    // Acesso a elementos
    // ------------------------------------------------------------------

    /// Acesso sem verificação de limites. Pré-condição: index < size().
    constexpr const_reference operator[](size_type index) const noexcept;

    /// Acesso com verificação de limites.
    /// @throws std::out_of_range se index >= size().
    const_reference at(size_type index) const;

    /// Pré-condição: !empty().
    constexpr const_reference front() const noexcept;

    /// Pré-condição: !empty().
    constexpr const_reference back() const noexcept;

    /// Acesso direto aos dados, sem cópia. Pode ser nulo se a view for vazia.
    constexpr const_pointer data() const noexcept;

    // ------------------------------------------------------------------
    // Iteração
    // ------------------------------------------------------------------

    constexpr const_iterator begin() const noexcept;
    constexpr const_iterator end() const noexcept;
    constexpr const_iterator cbegin() const noexcept;
    constexpr const_iterator cend() const noexcept;

    const_reverse_iterator rbegin() const noexcept;
    const_reverse_iterator rend() const noexcept;
    const_reverse_iterator crbegin() const noexcept;
    const_reverse_iterator crend() const noexcept;

    // ------------------------------------------------------------------
    // Subviews
    // ------------------------------------------------------------------

    /// Retorna uma nova view sobre [pos, pos + min(count, size() - pos)).
    /// Não copia dados: a subview aponta para a mesma memória.
    /// @throws std::out_of_range se pos > size().
    StringView substr(size_type pos = 0, size_type count = npos) const;

    // ------------------------------------------------------------------
    // Comparação
    // ------------------------------------------------------------------

    /// Comparação lexicográfica (byte a byte, como char_traits<char>).
    /// Retorna < 0, 0 ou > 0 se esta view for menor, igual ou maior que other.
    int compare(StringView other) const noexcept;

private:
    const char* data_ = nullptr;
    size_type   size_ = 0;
};

bool operator==(StringView lhs, StringView rhs) noexcept;
bool operator!=(StringView lhs, StringView rhs) noexcept;
bool operator<(StringView lhs, StringView rhs) noexcept;
bool operator<=(StringView lhs, StringView rhs) noexcept;
bool operator>(StringView lhs, StringView rhs) noexcept;
bool operator>=(StringView lhs, StringView rhs) noexcept;

}  // namespace StringView

#endif  // STRING_VIEW
