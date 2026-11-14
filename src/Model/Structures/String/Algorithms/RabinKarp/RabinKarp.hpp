#ifndef RABIN_KARP_HPP
#define RABIN_KARP_HPP

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace RabinKarp {

namespace detail {

constexpr std::uint64_t kBase = 256;
constexpr std::uint64_t kModulus = 1'000'000'007ULL;

std::uint64_t to_value(char c) noexcept;

std::uint64_t power_mod(std::size_t exponent) noexcept;

std::uint64_t hash_of(std::string_view text, std::size_t length) noexcept;

template <typename OnMatch>
void for_each_match(std::string_view text, std::string_view pattern, OnMatch on_match) {
    const std::size_t text_size = text.size();
    const std::size_t pattern_size = pattern.size();

    if (pattern_size == 0 || pattern_size > text_size) {
        return;
    }

    const std::uint64_t leading_weight = power_mod(pattern_size - 1);

    const std::uint64_t pattern_hash = hash_of(pattern, pattern_size);
    std::uint64_t window_hash = hash_of(text, pattern_size);

    const std::size_t last_start = text_size - pattern_size;
    for (std::size_t start = 0; start <= last_start; ++start) {
        if (window_hash == pattern_hash &&
            text.compare(start, pattern_size, pattern) == 0) {
            if (!on_match(start)) {
                return;
            }
        }

        if (start < last_start) {
            const std::uint64_t outgoing = (to_value(text[start]) * leading_weight) % kModulus;
            const std::uint64_t incoming = to_value(text[start + pattern_size]);

            window_hash = (window_hash + kModulus - outgoing) % kModulus;
            window_hash = (window_hash * kBase + incoming) % kModulus;
        }
    }
}

}  // namespace detail

std::vector<std::size_t> find_all(std::string_view text, std::string_view pattern);

std::optional<std::size_t> find_first(std::string_view text, std::string_view pattern);

}  // namespace RabinKarp

#endif  // RABIN_KARP_HPP
