#include "Levenshtein.hpp"

using LA = LevenArray;
using LV = LevenView;

LA::LevenshteinMatrix levenshtein_matrix(LV a, LV b) {
    const std::size_t n = a.size();
    const std::size_t m = b.size();

    LA::LevenshteinMatrix matrix =
        LA::Tables::make_levenshtein_matrix(n, m);

    for (std::size_t i = 0; i <= n; ++i) {
        matrix(i, 0) = i;
    }
    for (std::size_t j = 0; j <= m; ++j) {
        matrix(0, j) = j;
    }

    for (std::size_t i = 1; i <= n; ++i) {
        for (std::size_t j = 1; j <= m; ++j) {
            const std::size_t cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
            matrix(i, j) = std::min({matrix(i - 1, j) + 1,
                                     matrix(i, j - 1) + 1,
                                     matrix(i - 1, j - 1) + cost});
        }
    }

    return matrix;
}

std::size_t levenshtein_distance(LV a, LV b) {
    const LA::LevenshteinMatrix matrix = levenshtein_matrix(a, b);
    return matrix(a.size(), b.size());
}

double levenshtein_similarity(LV a, LV b) {
    const std::size_t max_len = std::max(a.size(), b.size());
    if (max_len == 0) {
        return 1.0;
    }
    return 1.0 - static_cast<double>(levenshtein_distance(a, b)) /
                     static_cast<double>(max_len);
}
