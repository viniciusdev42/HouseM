#include "BoyerMoore.hpp"

using BV = BoyerView;
using BA = BoyerArray;

/* pattern = objeto BoyerView ; pattern_ = std::string*/ 
BoyerMoore BoyerMoore(BV pattern) : pattern_(pattern.data(),pattern.size()){ // Converte StringView pra std::string 
    if (pattern_.empty()) {
        throw std::invalid_argument("BoyerMoore: o padrao nao pode ser vazio.");
    }
    build_bad_character_table();
    build_good_suffix_table();
}

std::size_t BoyerMoore::find_first(BV text) const noexcept {
    std::size_t result = npos;
    search(text, [&result](std::size_t position) {
        result = position;
        return false;
    });
    return result;
}

std::optional<std::size_t> BoyerMoore::find(BV text) const noexcept {
    const std::size_t position = find_first(text);
    if (position == npos) {
        return std::nullopt;
    }
    return position;
}

 BA find_all(BV text) const {
    BA positions;
    search(text, [&positions](std::size_t position) {
        positions.push_back(position);
        return true;
    });
    return positions;
}

bool BoyerMoore::contains(BV text) const noexcept {
    return find_first(text) != npos;
}

const std::string& BoyerMoore::pattern() const noexcept {
    return pattern_;
}

std::size_t BoyerMoore::to_index(char c) noexcept {
    return static_cast<unsigned char>(c);
}

void BoyerMoore::build_bad_character_table() {
    last_occurrence_.fill(-1);
    for (std::size_t i = 0; i < pattern_.size(); ++i) {
        last_occurrence_[to_index(pattern_[i])] = static_cast<std::ptrdiff_t>(i);
    }
}

void BoyerMoore::build_good_suffix_table() {
    const std::size_t m = pattern_.size();
    good_suffix_shift_.assign(m + 1, 0);
   BA border(m + 1, 0);

    std::size_t i = m;
    std::size_t j = m + 1;
    border[i] = j;
    while (i > 0) {
        while (j <= m && pattern_[i - 1] != pattern_[j - 1]) {
            if (good_suffix_shift_[j] == 0) {
                good_suffix_shift_[j] = j - i;
            }
            j = border[j];
        }
        --i;
        --j;
        border[i] = j;
    }

    j = border[0];
    for (i = 0; i <= m; ++i) {
        if (good_suffix_shift_[i] == 0) {
            good_suffix_shift_[i] = j;
        }
        if (i == j) {
            j = border[j];
        }
    }
}

std::ptrdiff_t BoyerMoore::bad_character_shift(std::size_t mismatch_index, char text_char) const noexcept {
    return static_cast<std::ptrdiff_t>(mismatch_index) - last_occurrence_[to_index(text_char)];
}

std::size_t find_first(BV text, BV pattern) {
    return BV(pattern).find_first(text);
}

BA find_all(BV text, BV pattern) {
    return BV(pattern).find_all(text);
}
