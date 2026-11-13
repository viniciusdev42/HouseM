#include "StringView.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>  // std::char_traits

namespace StringView {

namespace {
using Traits = std::char_traits<char>;
}  // namespace

StringView::StringView(const char* cString) noexcept
    : data_(cString),
      size_(cString != nullptr ? Traits::length(cString) : 0) {}

StringView::const_reference StringView::at(size_type index) const {
    if (index >= size_) {
        throw std::out_of_range(
            "StringView::at: index (" + std::to_string(index) +
            ") >= size (" + std::to_string(size_) + ")");
    }
    return data_[index];
}

StringView StringView::substr(size_type pos, size_type count) const {
    if (pos > size_) {
        throw std::out_of_range(
            "StringView::substr: pos (" + std::to_string(pos) +
            ") > size (" + std::to_string(size_) + ")");
    }
    const size_type available = size_ - pos;
    return StringView(data_ + pos, std::min(count, available));
}

int StringView::compare(StringView other) const noexcept {
    const size_type commonLength = std::min(size_, other.size_);

    // Evita passar ponteiros nulos a char_traits quando ambas são vazias.
    if (commonLength > 0) {
        const int result = Traits::compare(data_, other.data_, commonLength);
        if (result != 0) {
            return result;
        }
    }

    if (size_ == other.size_) return 0;
    return size_ < other.size_ ? -1 : 1;
}

bool operator==(StringView lhs, StringView rhs) noexcept {
    // Tamanhos diferentes dispensam a comparação do conteúdo.
    return lhs.size() == rhs.size() && lhs.compare(rhs) == 0;
}

bool operator!=(StringView lhs, StringView rhs) noexcept { return !(lhs == rhs); }
bool operator<(StringView lhs, StringView rhs) noexcept { return lhs.compare(rhs) < 0; }
bool operator<=(StringView lhs, StringView rhs) noexcept { return lhs.compare(rhs) <= 0; }
bool operator>(StringView lhs, StringView rhs) noexcept { return lhs.compare(rhs) > 0; }
bool operator>=(StringView lhs, StringView rhs) noexcept { return lhs.compare(rhs) >= 0; }

}  // namespace StringView
