#ifndef BOYER_MOORE_HPP
#define BOYER_MOORE_HPP

#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "src/Main/Model/Structures/String/Core/View/StringView.hpp"
#include "src/Main/Model/Structures/String/Core/Array/DynamicArray.hpp"

using BoyerView = StringView::StringView;
using BoyerArray = DynamicArray::IndexArray;

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

    // on_match(posicao) retorna true para continuar a busca e false para parar.
    // Definido no header porque e um template.
    template <typename OnMatch>
    void search(BoyerView text, OnMatch&& on_match) const {
        const std::size_t n = text.size();
        const std::size_t m = pattern_.size();
        if (n < m) {
            return;
        }

        const char* t = text.data();
        std::size_t shift_start = 0;

        while (shift_start <= n - m) {
            std::size_t j = m;
            while (j > 0 && pattern_[j - 1] == t[shift_start + j - 1]) {
                --j;
            }

            if (j == 0) {
                if (!on_match(shift_start)) {
                    return;
                }
                shift_start += good_suffix_shift_[0];
            } else {
                const std::ptrdiff_t bad = bad_character_shift(j - 1, t[shift_start + j - 1]);
                const std::size_t bad_shift = bad > 1 ? static_cast<std::size_t>(bad) : 1;
                shift_start += std::max(good_suffix_shift_[j], bad_shift);
            }
        }
    }
};

[[nodiscard]] std::size_t find_first(BoyerView text, BoyerView pattern);

[[nodiscard]] BoyerArray find_all(BoyerView text, BoyerView pattern);

}  // namespace BoyerMoore

#endif  // BOYER_MOORE_HPP
