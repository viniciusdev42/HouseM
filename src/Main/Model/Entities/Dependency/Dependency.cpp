#include "Dependency.hpp"

#include <stdexcept>

namespace Dependency {
 
using TaskId = ::Task::TaskId;

namespace {

/// Mistura de bits (estilo boost::hash_combine) para que pares simétricos,
/// como (1, 2) e (2, 1), não produzam o mesmo hash.
std::size_t combineHash(std::size_t seed, std::size_t value)  {
    constexpr std::size_t kGoldenRatio = static_cast<std::size_t>(0x9e3779b97f4a7c15ULL);
    return seed ^ (value + kGoldenRatio + (seed << 6) + (seed >> 2));
}

/// Valida o par de IDs, lançando std::invalid_argument se violar as invariantes.
void validatePair(TaskId prerequisiteId, TaskId dependentId) {
  if (!Dependency::isValidPair(prerequisiteId, dependentId)) {
        throw std::invalid_argument(
            "Dependencia invalida: ambos os IDs devem ser validos (nao vazios) e distintos entre si.");
    }

}  

} // namespace

Dependency::Dependency(TaskId prerequisiteId, TaskId dependentId)
    : prerequisiteId_(prerequisiteId), dependentId_(dependentId) {
    validatePair(prerequisiteId, dependentId);
}

/*isValid() é uma função de TaskId*/
bool Dependency::isValidPair(TaskId prerequisiteId, TaskId dependentId) {
    return prerequisiteId.isValid() &&
           dependentId.isValid() &&
           prerequisiteId != dependentId;
}

std::size_t Dependency::hashValue() const {
    const std::size_t seed = std::hash<std::string>{}(prerequisiteId_.value());
    return combineHash(seed, std::hash<std::string>{}(dependentId_.value()));
}

std::string Dependency::toString() const {
    return prerequisiteId_.value() + " -> " + dependentId_.value();
}

}  // namespace Dependency
