#include "Serializer.hpp"

#include <cstddef>
#include <cstdint>
#include <exception>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace {
 
// ---------------------------------------------------------------------------
// Auxiliares de escrita/leitura em little-endian (independentes da máquina).
// ---------------------------------------------------------------------------

  static void putU16(std::vector<std::uint8_t>& out, std::uint16_t v) {
        out.push_back(static_cast<std::uint8_t>(v & 0xFF));
        out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
    }

    static void putU32(std::vector<std::uint8_t>& out, std::uint32_t v) {
        for (int i = 0; i < 4; ++i) {
            out.push_back(static_cast<std::uint8_t>((v >> (8 * i)) & 0xFF));
        }
    }

    static bool getU8(const std::vector<std::uint8_t>& in, std::size_t size,
                      std::size_t& pos, std::uint8_t& v) {
        if (pos + 1 > size) return false;
        v = in[pos++];
        return true;
    }

    static bool getU16(const std::vector<std::uint8_t>& in, std::size_t size,
                       std::size_t& pos, std::uint16_t& v) {
        if (pos + 2 > size) return false;
        v = static_cast<std::uint16_t>(in[pos] | (in[pos + 1] << 8));
        pos += 2;
        return true;
    }

    static bool getU32(const std::vector<std::uint8_t>& in, std::size_t size,
                       std::size_t& pos, std::uint32_t& v) {
        if (pos + 4 > size) return false;
        v = 0;
        for (int i = 0; i < 4; ++i) {
            v |= static_cast<std::uint32_t>(in[pos + i]) << (8 * i);
        }
        pos += 4;
        return true;
    }
    
} // namespace

  /**
     * Serializa `node` e o insere em `page`.
     *
     * @param node          Nó a ser serializado.
     * @param childPageIds  pageId de cada filho, na mesma ordem dos filhos do
     *                      nó. Deve ter exatamente node.childCount() elementos.
     * @param page          Page onde o registro será inserido.
     * @param slotId        [out] Slot atribuído ao registro.
     * @return true em caso de sucesso; false se não houver espaço na Page.
     * @throws std::invalid_argument se childPageIds.size() != node.childCount().
     */

 static bool serialize(const BTreeNode& node, const std::vector<std::uint32_t>& childPageIds,
                            Page& page,std::uint16_t& slotId) {
              
  static_assert(std::is_integral<BTreeNode::Key>::value &&
                sizeof(BTreeNode::Key) == sizeof(std::int32_t),
                "Serializer espera BTreeNode::Key inteiro de 32 bits");

  if (childPageIds.size() != node.childCount()) {
           throw std::invalid_argument(
                "Serializer::serialize: childPageIds deve ter um pageId por filho");
        }

    const std::size_t numKeys = node.keyCount();
    const std::size_t numChildren = childPageIds.size();
    const std::size_t total = 1 + 4 + 2 + numKeys * 4 + 2 + numChildren * 4;

    if (total > PAGE_SIZE) {
            return false; // Nunca caberia em uma Page.
        }

    std::vector<std::uint8_t> buffer;
    buffer.reserve(total);

    buffer.push_back(node.isLeaf() ? 1 : 0);
    putU32(buffer, static_cast<std::uint32_t>(node.minDegree()));
    putU16(buffer, static_cast<std::uint16_t>(numKeys));
      for (std::size_t i = 0; i < numKeys; ++i) {
            putU32(buffer, static_cast<std::uint32_t>(node.getKey(i)));
        }
        putU16(buffer, static_cast<std::uint16_t>(numChildren));
      for (std::uint32_t id : childPageIds) {
           putU32(buffer, id);
        }

      return page.insertRecord(buffer.data(),static_cast<std::uint16_t>(buffer.size()), slotId);
    }

   /**
     * Reconstrói um BTreeNode a partir do registro em `slotId` de `page`.
     *
     * O nó devolvido contém o tipo (folha ou interno), o grau mínimo e as
     * chaves, mas ainda NÃO possui filhos: o chamador deve carregar cada
     * Page listada em `childPageIds` e anexá-la com insertChild().
     *
     * @param page          Page de onde ler o registro.
     * @param slotId        Slot do registro.
     * @param childPageIds  [out] pageIds dos filhos, em ordem.
     * @return o nó reconstruído, ou std::nullopt se o slot for inválido/removido
     *         ou se o conteúdo do registro estiver corrompido.
     */

   static std::optional<BTreeNode> deserialize(const Page& page,std::uint16_t slotId,
                                              std::vector<std::uint32_t>& childPageIds) {
     childPageIds.clear();

      std::vector<std::uint8_t> buffer(PAGE_SIZE);
      std::uint16_t size = 0;
      if (!page.getRecord(slotId, buffer.data(),
                            static_cast<std::uint16_t>(buffer.size()), size)) {
            return std::nullopt;
        }

        std::size_t pos = 0;
        std::uint8_t leafByte = 0;
        std::uint32_t minDegree = 0;
        std::uint16_t numKeys = 0;
        std::uint16_t numChildren = 0;

        if (!getU8(buffer, size, pos, leafByte) || leafByte > 1) return std::nullopt;
        if (!getU32(buffer, size, pos, minDegree)) return std::nullopt;
        if (!getU16(buffer, size, pos, numKeys)) return std::nullopt;

        std::vector<BTreeNode::Key> keys;
        keys.reserve(numKeys);
        for (std::uint16_t i = 0; i < numKeys; ++i) {
            std::uint32_t raw = 0;
            if (!getU32(buffer, size, pos, raw)) return std::nullopt;
            keys.push_back(static_cast<BTreeNode::Key>(static_cast<std::int32_t>(raw)));
        }

        if (!getU16(buffer, size, pos, numChildren)) return std::nullopt;

        std::vector<std::uint32_t> ids;
        ids.reserve(numChildren);
        for (std::uint16_t i = 0; i < numChildren; ++i) {
            std::uint32_t id = 0;
            if (!getU32(buffer, size, pos, id)) return std::nullopt;
            ids.push_back(id);
        }

        if (pos != size) return std::nullopt; // Bytes sobrando: registro corrompido.

        try {
            BTreeNode node(static_cast<std::size_t>(minDegree), leafByte == 1);
            for (const auto& key : keys) {
                node.insertKey(key);
            }
            childPageIds = std::move(ids);
            return node;
        } catch (const std::exception&) {
            // minDegree inválido, chaves duplicadas ou excesso de chaves.
            return std::nullopt;
        }
    }
