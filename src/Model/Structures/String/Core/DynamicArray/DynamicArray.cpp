#include "DynamicArray.hpp"

namespace DynamicArray {

// ============================================================================
// 2. Sequências de caracteres
// ============================================================================

namespace Text {

CharArray make_char_array(ArrayView content) {
    CharArray result;
    result.append(content.data(), content.size());
    return result;
}

void append_text(CharArray& target, ArrayView content) {
    target.append(content.data(), content.size());
}

ArrayView as_view(const CharArray& array) noexcept {
    return ArrayView(array.data(), array.size());
}

std::string to_string(const CharArray& array) {
    return std::string(array.data(), array.size());
}

CharArray substring(const CharArray& array, std::size_t position, std::size_t length) {
    if (position > array.size()) {
        throw std::out_of_range("Text::substring: posicao " + std::to_string(position) +
                                " maior que o tamanho " + std::to_string(array.size()));
    }
    const std::size_t effective = std::min(length, array.size() - position);
    CharArray result;
    result.append(array.data() + position, effective);
    return result;
}

}  // namespace Text

// ============================================================================
// 5. Tabelas auxiliares
// ============================================================================

namespace {

// Seguro porque modulus <= 2^32: os resíduos são < 2^32 e o produto cabe em 64 bits.
std::uint64_t pow_mod(std::uint64_t base, std::size_t exponent, std::uint64_t modulus) {
    std::uint64_t result = 1 % modulus;
    base %= modulus;
    while (exponent > 0) {
        if (exponent & 1) result = result * base % modulus;
        base = base * base % modulus;
        exponent >>= 1;
    }
    return result;
}

}  // namespace

namespace Tables {

KMPTable make_kmp_table(std::size_t pattern_length) {
    KMPTable table;
    table.failure.resize(pattern_length, 0);
    return table;
}

BoyerMooreTables make_boyer_moore_tables(std::size_t pattern_length) {
    if (pattern_length == std::numeric_limits<std::size_t>::max()) {
        throw std::length_error("make_boyer_moore_tables: tamanho excede o limite");
    }
    BoyerMooreTables tables;
    tables.bad_character.resize(BoyerMooreTables::kAlphabetSize, -1);
    tables.good_suffix_shift.resize(pattern_length + 1, 0);
    tables.suffix_border.resize(pattern_length + 1, 0);
    return tables;
}

RabinKarpTable make_rabin_karp_table(std::size_t text_length, std::size_t pattern_length,
                                     std::uint64_t base, std::uint64_t modulus) {
    constexpr std::uint64_t kMaxModulus = std::uint64_t{1} << 32;
    if (modulus == 0 || modulus > kMaxModulus) {
        throw std::invalid_argument("make_rabin_karp_table: modulo deve estar em [1, 2^32]");
    }
    RabinKarpTable table;
    table.base = base % modulus;
    table.modulus = modulus;
    table.high_order_factor =
        pattern_length == 0 ? 1 % modulus : pow_mod(table.base, pattern_length - 1, modulus);
    const std::size_t windows =
        text_length >= pattern_length ? text_length - pattern_length + 1 : 0;
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

}  // namespace DynamicArray
