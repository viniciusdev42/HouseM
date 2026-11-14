#ifndef RABIN_KARP
#define RABIN_KARP

#include <cstddef>
#include <cstdint>
#include <optional>

#include "src/Model/Structures/String/Core/StringView/StringView.hpp"
#include "src/Model/Structures/String/Core/DynamicArray/DynamicArray.hpp"

using KarpView = StringView::StringView;
using KarpArray = DynamicArray::DynamicArray;

namespace RabinKarp {

constexpr std::uint64_t kBase = 256;

constexpr std::uint64_t kModulus = 1'000'000'007ULL;

std::uint64_t to_value(char c) noexcept;

std::uint64_t power_mod(std::size_t exponent) noexcept;

std::uint64_t hash_of(KarpView text, std::size_t length) noexcept;

template <typename OnMatch> void for_each_match(KarpView text, KarpView pattern, OnMatch on_match);

KarpArray::IndexArray find_all(KarpView text, KarpView pattern);

std::optional<std::size_t> find_first(KarpView text, KarpView pattern);

}  // namespace RabinKarp

#endif  // RABIN_KARP
