#include "RabinKarp.hpp"

using KV = KarpView;
using KA = KarpArray;

namespace RabinKarp {

std::uint64_t to_value(char c) noexcept {
    return static_cast<unsigned char>(c);
}

std::uint64_t power_mod(std::size_t exponent) noexcept {
    std::uint64_t result = 1;
    std::uint64_t base = kBase % kModulus;
    while (exponent > 0) {
        if (exponent & 1U) {
            result = (result * base) % kModulus;
        }
        base = (base * base) % kModulus;
        exponent >>= 1U;
    }
    return result;
}

std::uint64_t hash_of(KV text, std::size_t length) noexcept {
    std::uint64_t hash = 0;
    for (std::size_t i = 0; i < length; ++i) {
        hash = (hash * kBase + to_value(text[i])) % kModulus;
    }
    return hash;
}

namespace detail {

// on_match(position) -> bool: retorne false para interromper a busca.
template <typename OnMatch>
void for_each_match(KV text, KV pattern, OnMatch on_match) {
    const std::size_t n = text.size();
    const std::size_t m = pattern.size();

    if (m == 0 || m > n) {
        return;
    }

    const std::uint64_t pattern_hash = hash_of(pattern, m);
    std::uint64_t window_hash = hash_of(text, m);
    const std::uint64_t high = power_mod(m - 1);  // kBase^(m-1) mod kModulus

    const std::size_t last = n - m;
    for (std::size_t i = 0; i <= last; ++i) {
        if (window_hash == pattern_hash && equals_at(text, pattern, i)) {
            if (!on_match(i)) {
                return;
            }
        }

        if (i < last) {
            const std::uint64_t removed = (to_value(text[i]) * high) % kModulus;
            window_hash = (window_hash + kModulus - removed) % kModulus;
            window_hash = (window_hash * kBase + to_value(text[i + m])) % kModulus;
        }
    }
}
} //namespace detail

KA find_all(KV text, KV pattern) {
   KA positions;
    detail::for_each_match(text, pattern, [&positions](std::size_t position) {
        positions.push_back(position);
        return true;
    });
    return positions;
}

std::optional<std::size_t> find_first(KV text, KV pattern) {
    std::optional<std::size_t> first_position;
    detail::for_each_match(text, pattern, [&first_position](std::size_t position) {
        first_position = position;
        return false;
    });
    return first_position;
}

} // namespace RabinKarp