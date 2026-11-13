#ifndef LEVENSHTEIN_HPP
#define LEVENSHTEIN_HPP

#include <algorithm>
#include <cstddef>
#include <string_view>
#include <vector>

namespace Levenshtein {

template <typename RandomIt1, typename RandomIt2>
std::size_t levenshtein_distance(RandomIt1 first1, RandomIt1 last1,
                                 RandomIt2 first2, RandomIt2 last2) {
    std::size_t n = static_cast<std::size_t>(last1 - first1);
    std::size_t m = static_cast<std::size_t>(last2 - first2);

    if (n < m) {
        return levenshtein_distance(first2, last2, first1, last1);
    }

    if (m == 0) {
        return n;
    }

    std::vector<std::size_t> previous(m + 1);
    std::vector<std::size_t> current(m + 1);

    for (std::size_t j = 0; j <= m; ++j) {
        previous[j] = j;
    }

    for (std::size_t i = 1; i <= n; ++i) {
        current[0] = i;

        for (std::size_t j = 1; j <= m; ++j) {
            const std::size_t cost = (first1[i - 1] == first2[j - 1]) ? 0 : 1;

            current[j] = std::min({
                previous[j] + 1,
                current[j - 1] + 1,
                previous[j - 1] + cost
            });
        }

        std::swap(previous, current);
    }

    return previous[m];
}

std::size_t levenshtein_distance(std::string_view a, std::string_view b);

double levenshtein_similarity(std::string_view a, std::string_view b);

}  // namespace Levenshtein

#endif  // LEVENSHTEIN_HPP
