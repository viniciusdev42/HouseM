#include "Levenshtein.hpp"

#include <vector>

namespace Levenshtein {

namespace {

// Duas linhas bastam para a distancia: memoria O(min(n, m)) em vez de O(n * m).
// `longer` percorre as linhas; `shorter` define as colunas.
std::size_t distance_two_rows(LevenView longer, LevenView shorter) {
    const std::size_t n = longer.size();
    const std::size_t m = shorter.size();

    std::vector<std::size_t> previous(m + 1);
    std::vector<std::size_t> current(m + 1);
    for (std::size_t j = 0; j <= m; ++j) {
        previous[j] = j;
    }

    for (std::size_t i = 1; i <= n; ++i) {
        current[0] = i;
        for (std::size_t j = 1; j <= m; ++j) {
            const std::size_t cost = (longer[i - 1] == shorter[j - 1]) ? 0 : 1;
            current[j] = std::min({previous[j] + 1,
                                   current[j - 1] + 1,
                                   previous[j - 1] + cost});
        }
        previous.swap(current);
    }
    return previous[m];
}

}

LevenArray levenshtein_matrix(LevenView a, LevenView b) {
    const std::size_t n = a.size();
    const std::size_t m = b.size();

    LevenArray matrix =
        DynamicArray::Tables::make_levenshtein_matrix(n, m);

    for (std::size_t i = 0; i <= n; ++i) {
        matrix(i, 0) = i;
    }
    for (std::size_t j = 0; j <= m; ++j) {
        matrix(0, j) = j;
    }

    for (std::size_t i = 1; i <= n; ++i) {
        for (std::size_t j = 1; j <= m; ++j) {
            const std::size_t cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
            // Variaveis size_t explicitas: std::min({...}) exige o mesmo tipo
            // em todos os elementos, e o tipo da celula da matriz nao e garantido.
            const std::size_t deletion = matrix(i - 1, j) + 1;
            const std::size_t insertion = matrix(i, j - 1) + 1;
            const std::size_t substitution = matrix(i - 1, j - 1) + cost;
            matrix(i, j) = std::min({deletion, insertion, substitution});
        }
    }

    return matrix;
}

std::size_t levenshtein_distance(LevenView a, LevenView b) {
    return (a.size() >= b.size()) ? distance_two_rows(a, b)
                                  : distance_two_rows(b, a);
}

double levenshtein_similarity(LevenView a, LevenView b) {
    const std::size_t max_len = std::max(a.size(), b.size());
    if (max_len == 0) {
        return 1.0;
    }
    return 1.0 - static_cast<double>(levenshtein_distance(a, b)) /
                     static_cast<double>(max_len);
}

} // namespace Levenshtein