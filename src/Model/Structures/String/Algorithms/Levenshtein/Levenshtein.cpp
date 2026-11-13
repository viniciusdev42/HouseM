#include "Levenshtein.hpp"

namespace Levenshtein {

std::size_t levenshtein_distance(std::string_view a, std::string_view b) {
    return levenshtein_distance(a.begin(), a.end(), b.begin(), b.end());
}

double levenshtein_similarity(std::string_view a, std::string_view b) {
    const std::size_t max_len = std::max(a.size(), b.size());
    if (max_len == 0) {
        return 1.0;
    }
    return 1.0 - static_cast<double>(levenshtein_distance(a, b)) /
                     static_cast<double>(max_len);
}

}  // namespace Levenshtein
