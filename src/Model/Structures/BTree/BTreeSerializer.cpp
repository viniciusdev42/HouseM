#include "BTreeSerializer.hpp"

#include <array>
#include <cstddef>
#include <fstream>
#include <istream>
#include <ostream>
#include <type_traits>

namespace BTree {

namespace {

// ---------------------------------------------------------------------------
// CRC-32 (polinômio 0xEDB88320, o mesmo do zlib/PNG)
// ---------------------------------------------------------------------------

std::uint32_t crc32(const std::uint8_t* data, std::size_t size) {
    static const std::array<std::uint32_t, 256> table = [] {
        std::array<std::uint32_t, 256> t{};
        for (std::uint32_t i = 0; i < 256; ++i) {
            std::uint32_t c = i;
            for (int k = 0; k < 8; ++k) {
                c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            }
            t[i] = c;
        }
        return t;
    }();

    std::uint32_t crc = 0xFFFFFFFFu;
    for (std::size_t i = 0; i < size; ++i) {
        crc = table[(crc ^ data[i]) & 0xFFu] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFu;
}

// ---------------------------------------------------------------------------
// Leitura/escrita de inteiros little-endian em buffers de tamanho fixo
// ---------------------------------------------------------------------------

class ByteWriter {
public:
    explicit ByteWriter(std::vector<std::uint8_t>& buffer) : buffer_(buffer) {}

    void seek(std::size_t offset) { offset_ = offset; }

    template <typename T>
    void writeUnsigned(T value) {
        static_assert(std::is_unsigned<T>::value, "T deve ser inteiro sem sinal");
        if (offset_ + sizeof(T) > buffer_.size()) {
            throw SerializationError("Escrita excede o tamanho do buffer");
        }
        for (std::size_t i = 0; i < sizeof(T); ++i) {
            buffer_[offset_++] = static_cast<std::uint8_t>((value >> (8 * i)) & 0xFFu);
        }
    }

    void writeInt64(std::int64_t value) {
        writeUnsigned<std::uint64_t>(static_cast<std::uint64_t>(value));
    }

private:
    std::vector<std::uint8_t>& buffer_;
    std::size_t offset_ = 0;
};

class ByteReader {
public:
    explicit ByteReader(const std::vector<std::uint8_t>& buffer) : buffer_(buffer) {}

    template <typename T>
    T readUnsigned() {
        static_assert(std::is_unsigned<T>::value, "T deve ser inteiro sem sinal");
        if (offset_ + sizeof(T) > buffer_.size()) {
            throw SerializationError("Leitura excede o tamanho do buffer");
        }
        T value = 0;
        for (std::size_t i = 0; i < sizeof(T); ++i) {
            value |= static_cast<T>(static_cast<T>(buffer_[offset_++]) << (8 * i));
        }
        return value;
    }

    std::int64_t readInt64() {
        return static_cast<std::int64_t>(readUnsigned<std::uint64_t>());
    }

private:
    const std::vector<std::uint8_t>& buffer_;
    std::size_t offset_ = 0;
};

// ---------------------------------------------------------------------------
// I/O em streams
// ---------------------------------------------------------------------------

void writeExactly(std::ostream& out, const std::vector<std::uint8_t>& bytes, const char* what) {
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
    if (!out) {
        throw SerializationError(std::string("Falha ao escrever ") + what);
    }
}

void readExactly(std::istream& in, std::vector<std::uint8_t>& bytes, const char* what) {
    in.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (static_cast<std::size_t>(in.gcount()) != bytes.size()) {
        throw SerializationError(std::string("Arquivo truncado ao ler ") + what);
    }
}

// ---------------------------------------------------------------------------
// Validações
// ---------------------------------------------------------------------------

std::uint64_t maxKeysPerNode(std::uint32_t minimumDegree) {
    return 2ull * minimumDegree - 1ull;
}

std::uint64_t requiredPayloadSize(NodeType type, std::uint64_t keyCount) {
    std::uint64_t size = kPageHeaderSize + keyCount * 16ull;
    if (type == NodeType::Internal) {
        size += (keyCount + 1ull) * 8ull;
    }
    return size;
}

void validateGeometry(std::uint32_t pageSize, std::uint32_t minimumDegree) {
    if (pageSize < kMinPageSize || pageSize > kMaxPageSize) {
        throw SerializationError("pageSize fora do intervalo permitido: " +
                                 std::to_string(pageSize));
    }
    if (minimumDegree < kMinMinimumDegree) {
        throw SerializationError("minimumDegree deve ser >= 2");
    }
    if (pageSize < BTreeSerializer::minimumPageSizeFor(minimumDegree)) {
        throw SerializationError("pageSize insuficiente para minimumDegree " +
                                 std::to_string(minimumDegree));
    }
}

void validateRoot(std::uint64_t rootPageId, std::uint64_t totalPages) {
    const bool valid = (totalPages == 0) ? (rootPageId == kInvalidPageId)
                                         : (rootPageId < totalPages);
    if (!valid) {
        throw SerializationError("rootPageId inconsistente com totalPages");
    }
}

void validateNode(const BTreeNode& node, std::uint32_t minimumDegree, bool isRoot,
                  std::uint64_t totalPages) {
    const std::string where = "Nó " + std::to_string(node.pageId) + ": ";
    const std::uint64_t keyCount = node.keys.size();

    if (node.values.size() != keyCount) {
        throw SerializationError(where + "keys e values com tamanhos diferentes");
    }
    if (keyCount > maxKeysPerNode(minimumDegree)) {
        throw SerializationError(where + "excede o máximo de 2t-1 chaves");
    }
    if (!isRoot && keyCount < minimumDegree - 1u) {
        throw SerializationError(where + "nó não-raiz com menos de t-1 chaves");
    }
    if (isRoot && node.nodeType == NodeType::Internal && keyCount == 0) {
        throw SerializationError(where + "raiz interna sem chaves");
    }
    for (std::size_t i = 1; i < node.keys.size(); ++i) {
        if (node.keys[i - 1] >= node.keys[i]) {
            throw SerializationError(where + "chaves não estão em ordem estritamente crescente");
        }
    }

    if (node.nodeType == NodeType::Leaf) {
        if (!node.children.empty()) {
            throw SerializationError(where + "folha não pode ter filhos");
        }
        return;
    }

    if (node.children.size() != keyCount + 1) {
        throw SerializationError(where + "nó interno deve ter keyCount + 1 filhos");
    }
    for (std::uint64_t child : node.children) {
        if (child >= totalPages || child == node.pageId) {
            throw SerializationError(where + "referência de filho inválida: " +
                                     std::to_string(child));
        }
    }
}

// ---------------------------------------------------------------------------
// Codificação / decodificação
// ---------------------------------------------------------------------------

std::vector<std::uint8_t> encodeFileHeader(const FileHeader& header, std::uint32_t pageSize) {
    std::vector<std::uint8_t> block(pageSize, 0);
    ByteWriter writer(block);
    writer.writeUnsigned<std::uint32_t>(header.magicNumber);
    writer.writeUnsigned<std::uint32_t>(header.version);
    writer.writeUnsigned<std::uint32_t>(header.pageSize);
    writer.writeUnsigned<std::uint32_t>(header.minimumDegree);
    writer.writeUnsigned<std::uint64_t>(header.rootPageId);
    writer.writeUnsigned<std::uint64_t>(header.totalPages);
    writer.writeUnsigned<std::uint64_t>(header.totalKeys);
    return block;
}

FileHeader decodeFileHeader(const std::vector<std::uint8_t>& bytes) {
    ByteReader reader(bytes);
    FileHeader header;
    header.magicNumber = reader.readUnsigned<std::uint32_t>();
    header.version = reader.readUnsigned<std::uint32_t>();
    header.pageSize = reader.readUnsigned<std::uint32_t>();
    header.minimumDegree = reader.readUnsigned<std::uint32_t>();
    header.rootPageId = reader.readUnsigned<std::uint64_t>();
    header.totalPages = reader.readUnsigned<std::uint64_t>();
    header.totalKeys = reader.readUnsigned<std::uint64_t>();
    return header;
}

std::vector<std::uint8_t> encodePage(const BTreeNode& node, std::uint32_t pageSize) {
    const std::uint64_t needed = requiredPayloadSize(node.nodeType, node.keys.size());
    if (needed > pageSize) {
        throw SerializationError("Nó " + std::to_string(node.pageId) +
                                 " não cabe em uma página de " + std::to_string(pageSize) +
                                 " bytes");
    }

    std::vector<std::uint8_t> page(pageSize, 0);
    ByteWriter writer(page);

    writer.writeUnsigned<std::uint64_t>(node.pageId);
    writer.writeUnsigned<std::uint8_t>(static_cast<std::uint8_t>(node.nodeType));
    writer.writeUnsigned<std::uint32_t>(static_cast<std::uint32_t>(node.keys.size()));
    writer.writeUnsigned<std::uint32_t>(0);  // checksum: preenchido após o payload

    for (std::int64_t key : node.keys) writer.writeInt64(key);
    for (std::uint64_t value : node.values) writer.writeUnsigned<std::uint64_t>(value);
    for (std::uint64_t child : node.children) writer.writeUnsigned<std::uint64_t>(child);

    const std::uint32_t checksum = crc32(page.data(), page.size());
    writer.seek(kChecksumOffset);
    writer.writeUnsigned<std::uint32_t>(checksum);
    return page;
}

BTreeNode decodePage(const std::vector<std::uint8_t>& page, std::uint64_t expectedPageId,
                     std::uint32_t minimumDegree) {
    const std::string where = "Página " + std::to_string(expectedPageId) + ": ";

    ByteReader reader(page);
    PageHeader header;
    header.pageId = reader.readUnsigned<std::uint64_t>();
    const std::uint8_t rawType = reader.readUnsigned<std::uint8_t>();
    header.keyCount = reader.readUnsigned<std::uint32_t>();
    header.checksum = reader.readUnsigned<std::uint32_t>();

    // O checksum é calculado com o próprio campo zerado.
    std::vector<std::uint8_t> copy = page;
    for (std::uint32_t i = 0; i < 4; ++i) copy[kChecksumOffset + i] = 0;
    if (crc32(copy.data(), copy.size()) != header.checksum) {
        throw SerializationError(where + "checksum inválido (página corrompida)");
    }

    if (header.pageId != expectedPageId) {
        throw SerializationError(where + "pageId gravado (" + std::to_string(header.pageId) +
                                 ") difere da posição no arquivo");
    }
    if (rawType != static_cast<std::uint8_t>(NodeType::Leaf) &&
        rawType != static_cast<std::uint8_t>(NodeType::Internal)) {
        throw SerializationError(where + "nodeType desconhecido: " + std::to_string(rawType));
    }
    header.nodeType = static_cast<NodeType>(rawType);

    if (header.keyCount > maxKeysPerNode(minimumDegree) ||
        requiredPayloadSize(header.nodeType, header.keyCount) > page.size()) {
        throw SerializationError(where + "keyCount incompatível com o tamanho da página");
    }

    BTreeNode node;
    node.pageId = header.pageId;
    node.nodeType = header.nodeType;
    node.keys.reserve(header.keyCount);
    node.values.reserve(header.keyCount);

    for (std::uint32_t i = 0; i < header.keyCount; ++i) node.keys.push_back(reader.readInt64());
    for (std::uint32_t i = 0; i < header.keyCount; ++i) {
        node.values.push_back(reader.readUnsigned<std::uint64_t>());
    }
    if (node.nodeType == NodeType::Internal) {
        node.children.reserve(static_cast<std::size_t>(header.keyCount) + 1);
        for (std::uint32_t i = 0; i <= header.keyCount; ++i) {
            node.children.push_back(reader.readUnsigned<std::uint64_t>());
        }
    }
    return node;
}

}  // namespace

// ---------------------------------------------------------------------------
// API pública
// ---------------------------------------------------------------------------

std::uint64_t BTreeSerializer::minimumPageSizeFor(std::uint32_t minimumDegree) {
    return requiredPayloadSize(NodeType::Internal, maxKeysPerNode(minimumDegree));
}

void BTreeSerializer::serialize(const BTreeSnapshot& snapshot, std::ostream& out) {
    validateGeometry(snapshot.pageSize, snapshot.minimumDegree);

    const std::uint64_t totalPages = snapshot.nodes.size();
    validateRoot(snapshot.rootPageId, totalPages);

    // Ordena por pageId garantindo que os IDs formam exatamente 0..N-1.
    std::vector<const BTreeNode*> ordered(snapshot.nodes.size(), nullptr);
    for (const BTreeNode& node : snapshot.nodes) {
        if (node.pageId >= totalPages) {
            throw SerializationError("pageId fora do intervalo 0..N-1: " +
                                     std::to_string(node.pageId));
        }
        if (ordered[node.pageId] != nullptr) {
            throw SerializationError("pageId duplicado: " + std::to_string(node.pageId));
        }
        ordered[node.pageId] = &node;
    }

    std::uint64_t totalKeys = 0;
    for (const BTreeNode* node : ordered) {
        validateNode(*node, snapshot.minimumDegree, node->pageId == snapshot.rootPageId,
                     totalPages);
        totalKeys += node->keys.size();
    }

    FileHeader header;
    header.pageSize = snapshot.pageSize;
    header.minimumDegree = snapshot.minimumDegree;
    header.rootPageId = snapshot.rootPageId;
    header.totalPages = totalPages;
    header.totalKeys = totalKeys;

    writeExactly(out, encodeFileHeader(header, snapshot.pageSize), "cabeçalho do arquivo");
    for (const BTreeNode* node : ordered) {
        writeExactly(out, encodePage(*node, snapshot.pageSize), "página");
    }
    out.flush();
    if (!out) {
        throw SerializationError("Falha ao finalizar a escrita");
    }
}

BTreeSnapshot BTreeSerializer::deserialize(std::istream& in) {
    std::vector<std::uint8_t> headerBytes(kFileHeaderSize);
    readExactly(in, headerBytes, "cabeçalho do arquivo");
    const FileHeader header = decodeFileHeader(headerBytes);

    if (header.magicNumber != kMagicNumber) {
        throw SerializationError("magicNumber inválido: não é um arquivo BTree");
    }
    if (header.version != kFormatVersion) {
        throw SerializationError("Versão não suportada: " + std::to_string(header.version));
    }
    validateGeometry(header.pageSize, header.minimumDegree);
    validateRoot(header.rootPageId, header.totalPages);

    // Descarta o padding do bloco do cabeçalho.
    std::vector<std::uint8_t> padding(header.pageSize - kFileHeaderSize);
    readExactly(in, padding, "padding do cabeçalho");

    BTreeSnapshot snapshot;
    snapshot.pageSize = header.pageSize;
    snapshot.minimumDegree = header.minimumDegree;
    snapshot.rootPageId = header.rootPageId;

    // Sem reserve(totalPages): um cabeçalho corrompido não deve forçar alocação gigante.
    std::vector<std::uint8_t> page(header.pageSize);
    std::uint64_t keysFound = 0;
    for (std::uint64_t id = 0; id < header.totalPages; ++id) {
        readExactly(in, page, "página");
        BTreeNode node = decodePage(page, id, header.minimumDegree);
        validateNode(node, header.minimumDegree, id == header.rootPageId, header.totalPages);
        keysFound += node.keys.size();
        snapshot.nodes.push_back(std::move(node));
    }

    if (keysFound != header.totalKeys) {
        throw SerializationError("totalKeys do cabeçalho (" + std::to_string(header.totalKeys) +
                                 ") difere do total encontrado (" + std::to_string(keysFound) +
                                 ")");
    }
    return snapshot;
}

void BTreeSerializer::saveToFile(const BTreeSnapshot& snapshot, const std::string& path) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        throw SerializationError("Não foi possível abrir para escrita: " + path);
    }
    serialize(snapshot, file);
}

BTreeSnapshot BTreeSerializer::loadFromFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw SerializationError("Não foi possível abrir para leitura: " + path);
    }
    return deserialize(file);
}

}  // namespace BTree
