#ifndef KMP_HPP
#define KMP_HPP

#include <cstddef> 

#include "src/Main/Model/Structures/String/Core/View/StringView.hpp"
#include "src/Main/Model/Structures/String/Core/Array/DynamicArray.hpp"

namespace Kmp {

using KView = StringView::StringView;
using DynamicK = DynamicArray::IndexArray;

DynamicK computeLPS(KView pattern);

DynamicK search(KView text, KView pattern);

}  // namespace Kmp

#endif  // KMP_HPP
