#ifndef PAGE_FILE
#define PAGE_FILE

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace PageFile {

using PageId = std::uint32_t;

inline constexpr std::size_t kPageSize = 4096;
inline constexpr std::size_t kHeaderSize = 8;  // pageId (4 bytes) + checksum (4 bytes)
inline constexpr std::size_t kPayloadSize = kPageSize - kHeaderSize;

using RawPage = std::array<std::byte, kPageSize>;
using PagePayload = std::array<std::byte, kPayloadSize>;

class PageFileError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

enum class PageStatus {
    Ok,
    OutOfRange,        // pageId além do fim do arquivo
    IoError,           // falha de leitura (inclui página truncada)
    IdMismatch,        // o pageId gravado no cabeçalho difere do solicitado
    ChecksumMismatch   // conteúdo corrompido
};

/**
 * Arquivo de páginas de tamanho fixo (kPageSize bytes).
 *
 * Layout de cada página em disco (little-endian):
 *   [0..3]  pageId    - identidade da página (detecta escritas em local errado)
 *   [4..7]  checksum  - CRC-32 calculado sobre pageId + payload
 *   [8..]   payload   - kPayloadSize bytes de dados do usuário
 *
 * A página N ocupa o offset N * kPageSize.
 */
class PageFile {
public:
    // Abre o arquivo; cria um arquivo vazio caso ele não exista.
    explicit PageFile(const std::filesystem::path& path);

    PageFile(const PageFile&) = delete;
    PageFile& operator=(const PageFile&) = delete;
    PageFile(PageFile&&) noexcept = default;
    PageFile& operator=(PageFile&&) noexcept = default;

    // Lê a página e copia o payload para `payload`.
    // Lança PageFileError se a página não existir ou estiver inválida.
    void readPage(PageId pageId, PagePayload& payload);

    // Grava o payload na página `pageId`, calculando o cabeçalho.
    // Se pageId > pageCount(), as páginas intermediárias ficam zeradas
    // (e, portanto, inválidas até serem escritas).
    void writePage(PageId pageId, const PagePayload& payload);

    // Retorna true somente se a página existe, o pageId do cabeçalho confere
    // e o checksum está correto. Nunca lança exceção por página inválida.
    bool validatePage(PageId pageId);

    // Quantidade de páginas (a última pode estar truncada).
    std::uint64_t pageCount();

private:
    PageStatus loadAndInspect(PageId pageId, RawPage& raw);

    std::filesystem::path path_;
    std::fstream file_;
};
}  // namespace PageFile
#endif
