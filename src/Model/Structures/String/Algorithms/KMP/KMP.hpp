#ifndef KMP_HPP
#define KMP_HPP

#include <cstddef> 

#include "src/Model/Structures/String/Core/StringView/StringView.hpp"
#include "src/Model/Structures/String/Core/DynamicArray/DynamicArray.hpp"

using KView = StringView::StringView;
using DynamicK = DynamicArray::IndexArray;

namespace KMP {

DynamicK computeLPS(KView pattern);

DynamicK search(KView text, KView pattern);

}  // namespace KMP

#endif  // KMP_HPP
