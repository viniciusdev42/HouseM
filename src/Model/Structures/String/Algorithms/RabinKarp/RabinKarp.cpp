#include "RabinKarp.hpp"

using KV = KarpView;
using KA = KarpArray;

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

KA::IndexArray find_all(KV text, KV pattern) {
   KA::IndexArray positions;
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

