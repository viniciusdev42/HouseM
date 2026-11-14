#include "KMP.hpp"

using KV = KView;
using DK = DynamicK;

DK::IndexArray computeLPS(KV pattern) {
    DK::IndexArray lps(pattern.size(), 0);

    if (pattern.empty()) {
        return lps;
    }

    std::size_t prefixLength = 0;
    for (std::size_t i = 1; i < pattern.size(); ++i) {
        while (prefixLength > 0 && pattern[i] != pattern[prefixLength]) {
            prefixLength = static_cast<std::size_t>(lps[prefixLength - 1]);
        }

        if (pattern[i] == pattern[prefixLength]) {
            ++prefixLength;
        }

        lps[i] = static_cast<int>(prefixLength);
    }

    return lps;
}

DK::IndexArray search(KV text, KV pattern) {
    DK::IndexArray occurrences;

    if (text.empty() || pattern.empty() || pattern.size() > text.size()) {
        return occurrences;
    }

   DK::IndexArrays lps = computeLPS(pattern);

    std::size_t matched = 0;
    for (std::size_t i = 0; i < text.size(); ++i) {
        while (matched > 0 && text[i] != pattern[matched]) {
            matched = static_cast<std::size_t>(lps[matched - 1]);
        }

        if (text[i] == pattern[matched]) {
            ++matched;
        }

        if (matched == pattern.size()) {
            occurrences.push_back(i + 1 - matched);
            matched = static_cast<std::size_t>(lps[matched - 1]);
        }
    }

    return occurrences;
}
