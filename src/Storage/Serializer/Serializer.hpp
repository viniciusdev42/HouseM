#ifndef SERIALIZER
#define SERIALIZER

#include <cstddef>
#include <cstdint>
#include <exception>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <vector>

#include "src/Model/Structures/BTree/BTreeNode.hpp"
#include "src/Storage/Page/Page.hpp"

/**
 * Serializa um BTreeNode como um registro de uma Page e o reconstrói depois.
 *
 * Como BTreeNode guarda ponteiros para filhos (que não fazem sentido em disco),
 * cada filho é representado pelo pageId da Page em que ele está armazenado.
 * Quem usa o Serializer (a BTree) informa esses pageIds ao serializar e
 * recebe-os de volta ao desserializar, para então carregar os filhos e
 * ligá-los ao nó com insertChild().
 *
 * Formato do registro (inteiros em little-endian, independente da máquina):
 *
 *   [ leaf: 1 byte ]
 *   [ minDegree: 4 bytes ]
 *   [ numKeys: 2 bytes ]
 *   [ keys: numKeys * 4 bytes ]
 *   [ numChildren: 2 bytes ]
 *   [ childPageIds: numChildren * 4 bytes ]
 */
class Serializer {
public:
  
  static bool serialize(const BTreeNode& node, const std::vector<std::uint32_t>& childPageIds,
                      Page& page,std::uint16_t& slotId); 
        

  static std::optional<BTreeNode> deserialize(const Page& page,std::uint16_t slotId,
                                              std::vector<std::uint32_t>& childPageIds);
 
private:
    Serializer() = delete;

#endif // SERIALIZER
