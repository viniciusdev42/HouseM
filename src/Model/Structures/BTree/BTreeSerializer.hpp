#ifndef BTREE_SERIALIZER
#define BTREE_SERIALIZER

#include <cstdint>
#include <iosfwd>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace BTree {

// ---------------------------------------------------------------------------
// Constantes do formato binário (todos os inteiros são little-endian)
// ---------------------------------------------------------------------------

inline constexpr std::uint32_t kMagicNumber = 0x45525442u;  // bytes 'B','T','R','E'
inline constexpr std::uint32_t kFormatVersion = 1u;

inline constexpr std::uint32_t kFileHeaderSize = 40u;  // 4+4+4+4+8+8+8
inline constexpr std::uint32_t kPageHeaderSize = 17u;  // 8+1+4+4
inline constexpr std::uint32_t kChecksumOffset = 13u;  // offset do checksum no cabeçalho da página

inline constexpr std::uint32_t kMinPageSize = 64u;
inline constexpr std::uint32_t kMaxPageSize = 16u * 1024u * 1024u;
inline constexpr std::uint32_t kMinMinimumDegree = 2u;

inline constexpr std::uint64_t kInvalidPageId = std::numeric_limits<std::uint64_t>::max();

// ---------------------------------------------------------------------------
// Tipos do formato
// ---------------------------------------------------------------------------

class SerializationError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

enum class NodeType : std::uint8_t {
    Leaf = 0,
    Internal = 1
};

// Cabeçalho do arquivo (ocupa o bloco 0, preenchido com zeros até pageSize).
//
// offset  tamanho  campo
//   0       4      magicNumber
//   4       4      version
//   8       4      pageSize
//  12       4      minimumDegree
//  16       8      rootPageId     (kInvalidPageId se a árvore estiver vazia)
//  24       8      totalPages
//  32       8      totalKeys
struct FileHeader {
    std::uint32_t magicNumber = kMagicNumber;
    std::uint32_t version = kFormatVersion;
    std::uint32_t pageSize = 0;
    std::uint32_t minimumDegree = 0;
    std::uint64_t rootPageId = kInvalidPageId;
    std::uint64_t totalPages = 0;
    std::uint64_t totalKeys = 0;
};

// Cabeçalho de cada página (seguido pelo payload e por padding de zeros).
//
// offset  tamanho  campo
//   0       8      pageId
//   8       1      nodeType
//   9       4      keyCount
//  13       4      checksum  (CRC-32 da página inteira com este campo zerado)
//
// Payload: keys[keyCount] (int64) | values[keyCount] (uint64) |
//          children[keyCount + 1] (uint64, somente nós internos)
struct PageHeader {
    std::uint64_t pageId = 0;
    NodeType nodeType = NodeType::Leaf;
    std::uint32_t keyCount = 0;
    std::uint32_t checksum = 0;
};

// Representação em memória de um nó. A página física de um nó fica na posição
// (pageId + 1) * pageSize do arquivo.
struct BTreeNode {
    std::uint64_t pageId = 0;
    NodeType nodeType = NodeType::Leaf;
    std::vector<std::int64_t> keys;
    std::vector<std::uint64_t> values;
    std::vector<std::uint64_t> children;  // IDs de página; vazio em folhas
};

// Estado completo da árvore. Os IDs dos nós devem formar o conjunto 0..N-1.
struct BTreeSnapshot {
    std::uint32_t pageSize = 4096;
    std::uint32_t minimumDegree = kMinMinimumDegree;
    std::uint64_t rootPageId = kInvalidPageId;
    std::vector<BTreeNode> nodes;
};

// ---------------------------------------------------------------------------
// Serializador
// ---------------------------------------------------------------------------

class BTreeSerializer {
public:
    // Menor pageSize capaz de armazenar um nó interno cheio (2t-1 chaves).
    static std::uint64_t minimumPageSizeFor(std::uint32_t minimumDegree);

    static void serialize(const BTreeSnapshot& snapshot, std::ostream& out);
    static BTreeSnapshot deserialize(std::istream& in);

    static void saveToFile(const BTreeSnapshot& snapshot, const std::string& path);
    static BTreeSnapshot loadFromFile(const std::string& path);
};

}  // namespace BTree

#endif  // BTREE_SERIALIZER
