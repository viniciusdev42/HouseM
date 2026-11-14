#ifndef BOYER_MOORE_HPP
#define BOYER_MOORE_HPP

#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace BoyerMoore {

class BoyerMoore {
public:
    static constexpr std::size_t npos = std::numeric_limits<std::size_t>::max();

    explicit BoyerMoore(std::string_view pattern);

    [[nodiscard]] std::size_t find_first(std::string_view text) const noexcept;

    [[nodiscard]] std::optional<std::size_t> find(std::string_view text) const noexcept;

    [[nodiscard]] std::vector<std::size_t> find_all(std::string_view text) const;

    [[nodiscard]] bool contains(std::string_view text) const noexcept;

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

    template <typename OnMatch>
    void search(std::string_view text, OnMatch&& on_match) const {
        const std::size_t m = pattern_.size();
        const std::size_t n = text.size();
        if (n < m) {
            return;
        }

        std::size_t shift = 0;
        while (shift <= n - m) {
            std::size_t j = m;
            while (j > 0 && pattern_[j - 1] == text[shift + j - 1]) {
                --j;
            }

            if (j == 0) {
                if (!on_match(shift)) {
                    return;
                }
                shift += good_suffix_shift_[0];
            } else {
                const std::ptrdiff_t bad_char = bad_character_shift(j - 1, text[shift + j - 1]);
                const auto good_suffix = static_cast<std::ptrdiff_t>(good_suffix_shift_[j]);
                shift += static_cast<std::size_t>(std::max(bad_char, good_suffix));
            }
        }
    }
};

[[nodiscard]] std::size_t find_first(std::string_view text, std::string_view pattern);

[[nodiscard]] std::vector<std::size_t> find_all(std::string_view text, std::string_view pattern);

}  // namespace BoyerMoore

#endif  // BOYER_MOORE_HPP
