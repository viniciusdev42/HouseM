#ifndef LEVENSHTEIN
#define LEVENSHTEIN

#include <algorithm>
#include <cstddef>

#include "src/Model/Structures/String/Core/StringView/StringView.hpp"
#include "src/Model/Structures/String/Core/DynamicArray/DynamicArray.hpp"

using LevenView = StringView::StringView;
using LevenArray = DynamicArray::DynamicArray;

namespace Levenshtein {

LevenArray::LevenshteinMatrix levenshtein_matrix(LevenView a, LevenView b);

std::size_t levenshtein_distance(LevenView a, LevenView b);

double levenshtein_similarity(LevenView a, LevenView b);

}  // namespace Levenshtein

#endif  // LEVENSHTEIN
