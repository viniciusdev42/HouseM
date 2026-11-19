#include "DiskManager.hpp"

#include <cstring>
#include <stdexcept>

namespace {

// ---------------------------------------------------------------------------
// Auxiliares de escrita/leitura em little-endian (independentes da máquina).
// ---------------------------------------------------------------------------

void putU16(std::uint8_t*& p, std::uint16_t v) {
    *p++ = static_cast<std::uint8_t>(v & 0xFF);
    *p++ = static_cast<std::uint8_t>((v >> 8) & 0xFF);
}

void putU32(std::uint8_t*& p, std::uint32_t v) {
    for (int i = 0; i < 4; ++i) {
        *p++ = static_cast<std::uint8_t>((v >> (8 * i)) & 0xFF);
    }
}

std::uint16_t getU16(const std::uint8_t*& p) {
    const std::uint16_t v = static_cast<std::uint16_t>(p[0] | (p[1] << 8));
    p += 2;
    return v;
}

std::uint32_t getU32(const std::uint8_t*& p) {
    std::uint32_t v = 0;
    for (int i = 0; i < 4; ++i) {
        v |= static_cast<std::uint32_t>(p[i]) << (8 * i);
    }
    p += 4;
    return v;
}

constexpr std::size_t SLOT_ENTRY_SIZE = 4;  // offset (2) + length (2), ver Page.hpp

} // namespace

DiskManager::DiskManager(const std::string& filePath)
    : filePath_(filePath) {
    const std::ios::openmode mode =
        std::ios::in | std::ios::out | std::ios::binary;

    file_.open(filePath_, mode);
    if (!file_.is_open()) {
        // O modo in|out não cria o arquivo; cria-se vazio e reabre-se para leitura/escrita.
        std::ofstream creator(filePath_, std::ios::binary);
        creator.close();
        file_.clear();
        file_.open(filePath_, mode);
    }

    if (!file_.is_open()) {
        throw std::runtime_error("DiskManager: não foi possível abrir ou criar o arquivo '" +
                                 filePath_ + "'");
    }
}

std::streamoff DiskManager::pageOffset(std::uint32_t pageId) {
    return static_cast<std::streamoff>(pageId) *
           static_cast<std::streamoff>(PAGE_DISK_SIZE);
}

// ---------------------------------------------------------------------------
// Serialização / desserialização
// ---------------------------------------------------------------------------

std::vector<std::uint8_t> DiskManager::serializePage(const Page& page) {
    const PageHeader& h = page.header_;

    std::vector<std::uint8_t> bytes(PAGE_DISK_SIZE);
    std::uint8_t* p = bytes.data();

    putU32(p, h.pageId);
    putU16(p, h.numRecords);
    putU16(p, h.freeSpace);
    putU16(p, h.numSlots);
    putU16(p, h.freeSpaceOffset);
    std::memcpy(p, h.pageData, PAGE_SIZE);

    return bytes;
}

bool DiskManager::deserializePage(const std::vector<std::uint8_t>& bytes,
                                  std::uint32_t expectedPageId, Page& page) {
    if (bytes.size() != PAGE_DISK_SIZE) {
        return false;
    }

    // Reconstrói em um header temporário para não corromper `page` se o
    // conteúdo for inválido.
    PageHeader h;
    const std::uint8_t* p = bytes.data();

    h.pageId          = getU32(p);
    h.numRecords      = getU16(p);
    h.freeSpace       = getU16(p);
    h.numSlots        = getU16(p);
    h.freeSpaceOffset = getU16(p);
    std::memcpy(h.pageData, p, PAGE_SIZE);

    // Verificações de integridade.
    if (h.pageId != expectedPageId) return false;  // Page errada ou "buraco" no arquivo

    const std::size_t directorySize = static_cast<std::size_t>(h.numSlots) * SLOT_ENTRY_SIZE;
    if (directorySize > PAGE_SIZE) return false;
    if (h.freeSpaceOffset > PAGE_SIZE - directorySize) return false;  // registros invadiriam o diretório
    if (h.numRecords > h.numSlots) return false;
    if (h.freeSpace > PAGE_SIZE) return false;

    page.header_ = h;
    return true;
}

// ---------------------------------------------------------------------------
// Leitura / escrita em disco
// ---------------------------------------------------------------------------

bool DiskManager::readPage(std::uint32_t pageId, Page& page) {
    file_.clear();  // limpa eofbit/failbit de operações anteriores

    file_.seekg(pageOffset(pageId), std::ios::beg);
    if (!file_) {
        return false;
    }

    std::vector<std::uint8_t> bytes(PAGE_DISK_SIZE);
    file_.read(reinterpret_cast<char*>(bytes.data()),
               static_cast<std::streamsize>(PAGE_DISK_SIZE));
    if (file_.gcount() != static_cast<std::streamsize>(PAGE_DISK_SIZE)) {
        file_.clear();
        return false;  // Page inexistente ou leitura parcial
    }

    return deserializePage(bytes, pageId, page);
}

bool DiskManager::writePage(const Page& page) {
    file_.clear();

    const std::vector<std::uint8_t> bytes = serializePage(page);

    file_.seekp(pageOffset(page.header_.pageId), std::ios::beg);
    if (!file_) {
        return false;
    }

    file_.write(reinterpret_cast<const char*>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()));
    file_.flush();  // garante que os dados saiam do buffer da stream
    return static_cast<bool>(file_);
}