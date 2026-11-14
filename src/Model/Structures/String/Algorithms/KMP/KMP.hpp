#ifndef KMP
#define KMP

#include <cstddef> 
#include <cstdint>

#include "src/Model/Structures/String/Core/StringView/StringView.hpp"
#include "src/Model/Structures/String/Core/DynamicArray/DynamicArray.hpp"

using KView = StringView::StringView;
using DynamicK = DynamicArray::DynamicArray;

namespace KMP {

DynamicK::IndexArray computeLPS(KView pattern);

DynamicK::IndexArray search(KView text, KView pattern);

}  // namespace KMP

#endif  // KMP
