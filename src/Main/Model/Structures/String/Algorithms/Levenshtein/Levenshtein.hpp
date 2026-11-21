#ifndef LEVENSHTEIN_HPP
#define LEVENSHTEIN_HPP

#include <algorithm>
#include <cstddef>

#include "src/Main/Model/Structures/String/Core/View/StringView.hpp"
#include "src/Main/Model/Structures/String/Core/Array/DynamicArray.hpp"

using LevenView = StringView::StringView;
using LevenArray = DynamicArray::LevenshteinMatrix;

namespace Levenshtein {

LevenArray levenshtein_matrix(LevenView a, LevenView b);

std::size_t levenshtein_distance(LevenView a, LevenView b);

double levenshtein_similarity(LevenView a, LevenView b);

}  // namespace Levenshtein

#endif  // LEVENSHTEIN_HPP
