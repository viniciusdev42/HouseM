#include "DiskManager.hpp"

#include <stdexcept>
#include <type_traits>

// A Page é gravada byte a byte; isso só é seguro para tipos trivialmente copiáveis.
static_assert(std::is_trivially_copyable<PageHeader>::value,
              "PageHeader deve ser trivialmente copiável para ser gravado em disco");

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

bool DiskManager::readPage(std::uint32_t pageId, Page& page) {
    file_.clear();  // limpa eofbit/failbit de operações anteriores

    file_.seekg(pageOffset(pageId), std::ios::beg);
    if (!file_) {
        return false;
    }

    // Lê para um buffer temporário para não corromper `page` em leituras parciais.
    PageHeader buffer;
    file_.read(reinterpret_cast<char*>(&buffer),
               static_cast<std::streamsize>(PAGE_DISK_SIZE));
    if (file_.gcount() != static_cast<std::streamsize>(PAGE_DISK_SIZE)) {
        file_.clear();
        return false;
    }

    page.header_ = buffer;
    return true;
}

bool DiskManager::writePage(const Page& page) {
    file_.clear();

    file_.seekp(pageOffset(page.header_.pageId), std::ios::beg);
    if (!file_) {
        return false;
    }

    file_.write(reinterpret_cast<const char*>(&page.header_),
                static_cast<std::streamsize>(PAGE_DISK_SIZE));
    file_.flush();  // garante que os dados saiam do buffer da stream
    return static_cast<bool>(file_);
}
