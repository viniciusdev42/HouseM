#ifndef KMP_HPP
#define KMP_HPP

#include <cstddef>
#include <string_view>
#include <vector>

namespace KMP {

std::vector<int> computeLPS(std::string_view pattern);

std::vector<std::size_t> search(std::string_view text, std::string_view pattern);

}  // namespace KMP

#endif  // KMP_HPP
