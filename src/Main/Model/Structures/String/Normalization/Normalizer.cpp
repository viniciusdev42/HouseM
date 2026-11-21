#include "Normalizer.hpp"

namespace Normalizer {

namespace Details {

bool inRange(char32_t c, char32_t low, char32_t high) {
    return c >= low && c <= high;
}

std::u32string decodeUtf8(NormalizerView input) {
    static constexpr char32_t kMinByLength[] = {0, 0, 0x80, 0x800, 0x10000};
    const char* bytes = input.data();
    const std::size_t inputSize = input.size();

    std::u32string output;
    output.reserve(inputSize);

    std::size_t i = 0;
    while (i < inputSize) {
        const auto lead = static_cast<unsigned char>(bytes[i]);
        char32_t codePoint = 0;
        std::size_t length = 0;

        if (lead < 0x80) {
            codePoint = lead;
            length = 1;
        } else if (lead >= 0xC2 && lead <= 0xDF) {
            codePoint = lead & 0x1Fu;
            length = 2;
        } else if (lead >= 0xE0 && lead <= 0xEF) {
            codePoint = lead & 0x0Fu;
            length = 3;
        } else if (lead >= 0xF0 && lead <= 0xF4) {
            codePoint = lead & 0x07u;
            length = 4;
        } else {
            throw std::invalid_argument("UTF-8 invalido na posicao " + std::to_string(i));
        }

        if (i + length > inputSize) {
            throw std::invalid_argument("UTF-8 truncado na posicao " + std::to_string(i));
        }

        for (std::size_t k = 1; k < length; ++k) {
            const auto next = static_cast<unsigned char>(bytes[i + k]);
            if ((next & 0xC0u) != 0x80u) {
                throw std::invalid_argument("UTF-8 invalido na posicao " + std::to_string(i + k));
            }
            codePoint = (codePoint << 6) | (next & 0x3Fu);
        }

        const bool surrogate = codePoint >= 0xD800 && codePoint <= 0xDFFF;
        if (codePoint < kMinByLength[length] || codePoint > 0x10FFFF || surrogate) {
            throw std::invalid_argument("UTF-8 invalido na posicao " + std::to_string(i));
        }

        output.push_back(codePoint);
        i += length;
    }
    return output;
}

NormalizerBuffer encodeUtf8(const std::u32string& input) {
    NormalizerBuffer output;
    output.reserve(input.size() * 2);

    for (const char32_t cp : input) {
        if (cp < 0x80) {
            output.pushBack(static_cast<char>(cp));
        } else if (cp < 0x800) {
            output.pushBack(static_cast<char>(0xC0 | (cp >> 6)));
            output.pushBack(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            output.pushBack(static_cast<char>(0xE0 | (cp >> 12)));
            output.pushBack(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            output.pushBack(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            output.pushBack(static_cast<char>(0xF0 | (cp >> 18)));
            output.pushBack(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            output.pushBack(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            output.pushBack(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }
    return output;
}

const std::unordered_map<char32_t, std::pair<char32_t, char32_t>>& decompositionMap() {
    static const auto map = [] {
        std::unordered_map<char32_t, std::pair<char32_t, char32_t>> result;
        result.reserve(std::size(kDecompositions));
        for (const auto& entry : kDecompositions) {
            result.emplace(entry.composed, std::make_pair(entry.first, entry.second));
        }
        return result;
    }();
    return map;
}

const std::unordered_map<std::uint64_t, char32_t>& compositionMap() {
    static const auto map = [] {
        std::unordered_map<std::uint64_t, char32_t> result;
        result.reserve(std::size(kDecompositions));
        for (const auto& entry : kDecompositions) {
            const std::uint64_t key =
                (static_cast<std::uint64_t>(entry.first) << 32) |
                static_cast<std::uint64_t>(entry.second);
            result.emplace(key, entry.composed);
        }
        return result;
    }();
    return map;
}

const std::unordered_map<char32_t, std::u32string>& caseFoldMap() {
    static const std::unordered_map<char32_t, std::u32string> map = {
        {0x00DF, U"ss"},
        {0x0130, U"i\u0307"},
        {0x0178, U"\u00FF"},
        {0x017F, U"s"},
        {0x01AF, U"\u01B0"},
        {0x01F4, U"\u01F5"},
        {0x1E9E, U"ss"},
        {0x1E9E, U"ss"},
        // Excecoes notaveis em Latin Extended Additional
        {0x1E9A, U"a\u02BE"},
        {0x1E9B, U"\u1E61"},
        {0x1E9C, U"\u1E61"},
        {0x1E9D, U"\u1E61"},
    };
    return map;
}

bool isCombiningMark(char32_t c) {
    return inRange(c, 0x0300, 0x036F) ||
           inRange(c, 0x1AB0, 0x1AFF) ||
           inRange(c, 0x1DC0, 0x1DFF) ||
           inRange(c, 0xFE20, 0xFE2F);
}

int combiningClass(char32_t c) {
    if (!isCombiningMark(c)) return 0;
    if (c == 0x031B) return 216;
    if (c == 0x0323 || c == 0x0326) return 220;
    if (c == 0x0327 || c == 0x0328) return 202;
    return 230;
}

void decomposeInto(char32_t c, std::u32string& output) {
    const auto& map = decompositionMap();
    const auto it = map.find(c);
    if (it == map.end()) {
        output.push_back(c);
        return;
    }
    decomposeInto(it->second.first, output);
    decomposeInto(it->second.second, output);
}

std::u32string decompose(const std::u32string& input) {
    std::u32string output;
    output.reserve(input.size() + input.size() / 4);
    for (const char32_t c : input) {
        decomposeInto(c, output);
    }

    std::size_t i = 0;
    while (i < output.size()) {
        if (combiningClass(output[i]) == 0) {
            ++i;
            continue;
        }
        std::size_t j = i;
        while (j < output.size() && combiningClass(output[j]) != 0) {
            ++j;
        }
        std::stable_sort(output.begin() + static_cast<std::ptrdiff_t>(i),
                         output.begin() + static_cast<std::ptrdiff_t>(j),
                         [](char32_t a, char32_t b) { return combiningClass(a) < combiningClass(b); });
        i = j;
    }
    return output;
}

std::u32string compose(const std::u32string& input) {
    const auto& map = compositionMap();
    const std::size_t kNoStarter = static_cast<std::size_t>(-1);

    std::u32string result;
    result.reserve(input.size());
    std::size_t starterIndex = kNoStarter;
    int lastClass = 0;
    bool blocked = false;

    for (const char32_t c : input) {
        const int currentClass = combiningClass(c);

        if (starterIndex != kNoStarter && currentClass != 0) {
            const bool adjacent = (result.size() - 1 == starterIndex);
            if (!blocked && (adjacent || lastClass < currentClass)) {
                const std::uint64_t key =
                    (static_cast<std::uint64_t>(result[starterIndex]) << 32) |
                    static_cast<std::uint64_t>(c);
                const auto it = map.find(key);
                if (it != map.end()) {
                    result[starterIndex] = it->second;
                    continue;
                }
            }
            if (currentClass >= lastClass && lastClass != 0) {
                blocked = true;
            }
            if (currentClass > lastClass) {
                lastClass = currentClass;
            }
            result.push_back(c);
            continue;
        }

        if (currentClass == 0) {
            starterIndex = result.size();
            lastClass = 0;
            blocked = false;
        } else {
            lastClass = currentClass;
        }
        result.push_back(c);
    }
    return result;
}

void appendCaseFolded(char32_t c, std::u32string& output) {
    if (c < 0x80) {
        output.push_back((c >= U'A' && c <= U'Z') ? static_cast<char32_t>(c + 0x20) : c);
        return;
    }

    const auto& special = caseFoldMap();
    const auto it = special.find(c);
    if (it != special.end()) {
        output.append(it->second);
        return;
    }

    if (inRange(c, 0x00C0, 0x00DE) && c != 0x00D7) {
        output.push_back(static_cast<char32_t>(c + 0x20));
        return;
    }

    const bool upperIsEven =
        inRange(c, 0x0100, 0x012F) || inRange(c, 0x0132, 0x0137) || inRange(c, 0x014A, 0x0177) ||
        inRange(c, 0x01A0, 0x01A5) || inRange(c, 0x01DE, 0x01EF) || inRange(c, 0x01F8, 0x021F) ||
        inRange(c, 0x1EA0, 0x1EFF);
    if (upperIsEven) {
        output.push_back((c & 1u) == 0 ? static_cast<char32_t>(c + 1) : c);
        return;
    }

    const bool upperIsOdd =
        inRange(c, 0x0139, 0x0148) || inRange(c, 0x0179, 0x017E) || inRange(c, 0x01CD, 0x01DC);
    if (upperIsOdd) {
        output.push_back((c & 1u) != 0 ? static_cast<char32_t>(c + 1) : c);
        return;
    }

    output.push_back(c);
}

std::u32string applyUnicodeForm(const std::u32string& input, UnicodeForm form) {
    const std::u32string decomposed = decompose(input);
    return form == UnicodeForm::NFC ? compose(decomposed) : decomposed;
}

std::u32string applyCaseFolding(const std::u32string& input) {
    std::u32string output;
    output.reserve(input.size());
    for (const char32_t c : input) {
        appendCaseFolded(c, output);
    }
    return output;
}

std::u32string applyDiacriticRemoval(const std::u32string& input) {
    const std::u32string decomposed = decompose(input);
    std::u32string stripped;
    stripped.reserve(decomposed.size());
    for (const char32_t c : decomposed) {
        if (!isCombiningMark(c)) {
            stripped.push_back(c);
        }
    }
    return compose(stripped);
}

}  // namespace Details

Normalizer::Normalizer(UnicodeForm form) : form_(form) {}

NormalizerBuffer Normalizer::normalizeUnicode(NormalizerView utf8Text) const {
    return Details::encodeUtf8(Details::applyUnicodeForm(Details::decodeUtf8(utf8Text), form_));
}

NormalizerBuffer Normalizer::caseFold(NormalizerView utf8Text) const {
    return Details::encodeUtf8(Details::applyCaseFolding(Details::decodeUtf8(utf8Text)));
}

NormalizerBuffer Normalizer::removeDiacritics(NormalizerView utf8Text) const {
    return Details::encodeUtf8(Details::applyDiacriticRemoval(Details::decodeUtf8(utf8Text)));
}

NormalizerBuffer Normalizer::normalize(NormalizerView utf8Text) const {
    std::u32string text = Details::decodeUtf8(utf8Text);
    text = Details::applyCaseFolding(text);
    text = Details::decompose(text);

    std::u32string stripped;
    stripped.reserve(text.size());
    for (const char32_t c : text) {
        if (!Details::isCombiningMark(c)) {
            stripped.push_back(c);
        }
    }

    text = Details::applyUnicodeForm(stripped, form_);
    return Details::encodeUtf8(text);
}

bool Normalizer::equivalent(NormalizerView lhs, NormalizerView rhs) const {
    const NormalizerBuffer left = normalize(lhs);
    const NormalizerBuffer right = normalize(rhs);
    return left.view() == right.view();
}

}  // namespace Normalizer