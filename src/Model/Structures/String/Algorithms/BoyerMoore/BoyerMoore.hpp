#ifndef BOYER_MOORE
#define BOYER_MOORE

#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "src/Model/Structures/String/Core/StringView/StringView.hpp"
#include "src/Model/Structures/String/Core/DynamicArray/DynamicArray.hpp"

using BoyerView = StringView::StringView;
using BoyerArray = DynamicArray::DynamicArray;

namespace BoyerMoore {
   
class BoyerMoore {
public:
    static constexpr std::size_t npos = std::numeric_limits<std::size_t>::max();

    explicit BoyerMoore(BoyerView pattern);

    [[nodiscard]] std::size_t find_first(BoyerView text) const noexcept;

    [[nodiscard]] std::optional<std::size_t> find(BoyerView text) const noexcept;

    [[nodiscard]] BoyerArray find_all(BoyerView text) const;

    [[nodiscard]] bool contains(BoyerView text) const noexcept;

    [[nodiscard]] const std::string& pattern() const noexcept;

private:
    static constexpr std::size_t kAlphabetSize = 
    static_cast<std::size_t>(std::numeric_limits<unsigned char>::max()) + 1;

    std::string pattern_;
    
    std::array<std::ptrdiff_t, kAlphabetSize> last_occurrence_{};
    
    std::vector<std::size_t> good_suffix_shift_;

    static std::size_t to_index(char c) noexcept;

    void build_bad_character_table();

    void build_good_suffix_table();

    std::ptrdiff_t bad_character_shift(std::size_t mismatch_index, char text_char) const noexcept;

    template <typename OnMatch> void search(BoyerView text, OnMatch&& on_match) const;
};

[[nodiscard]] std::size_t find_first(BoyerView text, BoyerView pattern);

[[nodiscard]] BoyerArray find_all(BoyerView text, BoyerView pattern);

}  // namespace BoyerMoore

#endif  // BOYER_MOORE
