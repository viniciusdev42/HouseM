#include "Kmp.hpp"

using Kv = KView;
using Dk = DynamicK;

namespace Kmp {

Dk computeLPS(Kv pattern) {
    Dk lps(pattern.size(), 0);

    if (pattern.empty()) {
        return lps;
    }

    std::size_t prefixLength = 0;
    for (std::size_t i = 1; i < pattern.size(); ++i) {
        while (prefixLength > 0 && pattern[i] != pattern[prefixLength]) {
            prefixLength = lps[prefixLength - 1];
        }

        if (pattern[i] == pattern[prefixLength]) {
            ++prefixLength;
        }

        lps[i] = prefixLength;
    }

    return lps;
}

Dk search(Kv text, Kv pattern) {
    Dk occurrences;

    if (text.empty() || pattern.empty() || pattern.size() > text.size()) {
        return occurrences;
    }

   Dk lps = computeLPS(pattern);

    std::size_t matched = 0;
    for (std::size_t i = 0; i < text.size(); ++i) {
        while (matched > 0 && text[i] != pattern[matched]) {
            matched = lps[matched - 1];
        }

        if (text[i] == pattern[matched]) {
            ++matched;
        }

        if (matched == pattern.size()) {
            occurrences.push_back(i + 1 - matched);
            matched = lps[matched - 1];
        }
    }

    return occurrences;
}

} // namespace Kmp