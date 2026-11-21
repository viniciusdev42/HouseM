#ifndef RABIN_KARP_HPP
#define RABIN_KARP_HPP

#include <cstddef>
#include <cstdint>
#include <optional>

#include "src/Main/Model/Structures/String/Core/View/StringView.hpp"
#include "src/Main/Model/Structures/String/Core/Array/DynamicArray.hpp"

using KarpView = StringView::StringView;
using KarpArray = DynamicArray::IndexArray;

namespace RabinKarp {

constexpr std::uint64_t kBase = 256;

constexpr std::uint64_t kModulus = 1'000'000'007ULL;

std::uint64_t to_value(char c) noexcept;

std::uint64_t power_mod(std::size_t exponent) noexcept;

std::uint64_t hash_of(KarpView text, std::size_t length) noexcept;

namespace detail {

// Compara caractere a caractere: necessario porque hashes iguais
// nao garantem strings iguais (colisao).
inline bool equals_at(KarpView text, KarpView pattern, std::size_t position) noexcept {
    for (std::size_t i = 0; i < pattern.size(); ++i) {
        if (text[position + i] != pattern[i]) {
            return false;
        }
    }
    return true;
}

template <typename OnMatch> void for_each_match(KarpView text, KarpView pattern, OnMatch on_match);

} // namespace detail

KarpArray find_all(KarpView text, KarpView pattern);

std::optional<std::size_t> find_first(KarpView text, KarpView pattern);

}  // namespace RabinKarp

#endif  // RABIN_KARP_HPP
