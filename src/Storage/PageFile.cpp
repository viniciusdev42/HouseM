#include "PageFile.hpp"

#include <algorithm>
#include <string>

namespace PageFile {

namespace {

constexpr std::size_t kIdOffset = 0;
constexpr std::size_t kChecksumOffset = 4;

void storeU32(std::byte* dst, std::uint32_t value) {
    for (std::size_t i = 0; i < 4; ++i) {
        dst[i] = static_cast<std::byte>((value >> (8 * i)) & 0xFFu);
    }
}

std::uint32_t loadU32(const std::byte* src) {
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < 4; ++i) {
        value |= static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(src[i])) << (8 * i);
    }
    return value;
}

std::uint32_t crc32Update(std::uint32_t crc, const std::byte* data, std::size_t size) {
    for (std::size_t i = 0; i < size; ++i) {
        crc ^= std::to_integer<std::uint8_t>(data[i]);
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
        }
    }
    return crc;
}

// O pageId entra no cálculo para que uma página copiada para outro
// local do arquivo seja detectada como inválida.
std::uint32_t computeChecksum(PageId pageId, const std::byte* payload) {
    std::byte idBytes[4];
    storeU32(idBytes, pageId);

    std::uint32_t crc = 0xFFFFFFFFu;
    crc = crc32Update(crc, idBytes, sizeof(idBytes));
    crc = crc32Update(crc, payload, kPayloadSize);
    return ~crc;
}

std::streamoff pageOffset(PageId pageId) {
    return static_cast<std::streamoff>(pageId) * static_cast<std::streamoff>(kPageSize);
}

std::string describe(PageStatus status, PageId pageId) {
    const std::string prefix = "Página " + std::to_string(pageId) + ": ";
    switch (status) {
        case PageStatus::OutOfRange:       return prefix + "fora dos limites do arquivo";
        case PageStatus::IoError:          return prefix + "falha de E/S ou página truncada";
        case PageStatus::IdMismatch:       return prefix + "PageId do cabeçalho não confere";
        case PageStatus::ChecksumMismatch: return prefix + "checksum inválido (dados corrompidos)";
        case PageStatus::Ok:               break;
    }
    return prefix + "estado desconhecido";
}

}  // namespace

PageFile::PageFile(const std::filesystem::path& path) : path_(path) {
    if (!std::filesystem::exists(path_)) {
        std::ofstream create(path_, std::ios::binary);
        if (!create) {
            throw PageFileError("Não foi possível criar o arquivo: " + path_.string());
        }
    }

    file_.open(path_, std::ios::in | std::ios::out | std::ios::binary);
    if (!file_.is_open()) {
        throw PageFileError("Não foi possível abrir o arquivo: " + path_.string());
    }
}

std::uint64_t PageFile::pageCount() {
    file_.clear();
    file_.seekg(0, std::ios::end);
    const std::streamoff size = file_.tellg();
    if (size < 0) {
        throw PageFileError("Não foi possível obter o tamanho do arquivo: " + path_.string());
    }
    // Arredonda para cima para que uma página truncada seja detectada na leitura.
    return (static_cast<std::uint64_t>(size) + kPageSize - 1) / kPageSize;
}

PageStatus PageFile::loadAndInspect(PageId pageId, RawPage& raw) {
    if (pageId >= pageCount()) {
        return PageStatus::OutOfRange;
    }

    file_.clear();
    file_.seekg(pageOffset(pageId), std::ios::beg);
    file_.read(reinterpret_cast<char*>(raw.data()), static_cast<std::streamsize>(kPageSize));
    if (file_.gcount() != static_cast<std::streamsize>(kPageSize)) {
        file_.clear();
        return PageStatus::IoError;
    }

    if (loadU32(raw.data() + kIdOffset) != pageId) {
        return PageStatus::IdMismatch;
    }

    const std::uint32_t stored = loadU32(raw.data() + kChecksumOffset);
    if (stored != computeChecksum(pageId, raw.data() + kHeaderSize)) {
        return PageStatus::ChecksumMismatch;
    }
    return PageStatus::Ok;
}

void PageFile::readPage(PageId pageId, PagePayload& payload) {
    RawPage raw{};
    const PageStatus status = loadAndInspect(pageId, raw);
    if (status != PageStatus::Ok) {
        throw PageFileError(describe(status, pageId));
    }
    std::copy_n(raw.begin() + kHeaderSize, kPayloadSize, payload.begin());
}

void PageFile::writePage(PageId pageId, const PagePayload& payload) {
    RawPage raw{};
    storeU32(raw.data() + kIdOffset, pageId);
    storeU32(raw.data() + kChecksumOffset, computeChecksum(pageId, payload.data()));
    std::copy(payload.begin(), payload.end(), raw.begin() + kHeaderSize);

    file_.clear();
    file_.seekp(pageOffset(pageId), std::ios::beg);
    file_.write(reinterpret_cast<const char*>(raw.data()), static_cast<std::streamsize>(kPageSize));
    file_.flush();
    if (!file_) {
        file_.clear();
        throw PageFileError("Página " + std::to_string(pageId) + ": falha ao gravar no disco");
    }
}

bool PageFile::validatePage(PageId pageId) {
    RawPage raw{};
    return loadAndInspect(pageId, raw) == PageStatus::Ok;
}

}  // namespace PageFile
