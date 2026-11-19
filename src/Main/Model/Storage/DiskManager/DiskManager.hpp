#ifndef  DISK_MANAGER_HPP
#define  DISK_MANAGER_HPP

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

#include "src/Storage/Page/Page.hpp"

/**
 * Gerencia a leitura e escrita de Pages em um arquivo de dados em disco.
 *
 * As Pages são gravadas de forma contígua: a Page de identificador N ocupa
 * os bytes [N * PAGE_DISK_SIZE, (N + 1) * PAGE_DISK_SIZE) do arquivo.
 *
 * Escrita: Page -> serializePage() -> buffer de bytes -> arquivo.
 * Leitura: arquivo -> buffer de bytes -> deserializePage() -> Page.
 *
 * Formato de uma Page em disco (inteiros em little-endian, independente da
 * máquina e do padding de PageHeader):
 *
 *   [ pageId:          4 bytes ]
 *   [ numRecords:      2 bytes ]
 *   [ freeSpace:       2 bytes ]
 *   [ numSlots:        2 bytes ]
 *   [ freeSpaceOffset: 2 bytes ]
 *   [ pageData:        PAGE_SIZE bytes ]
 *
 * REQUISITO: a classe Page deve declarar `friend class DiskManager;`, pois o
 * DiskManager precisa acessar o header_ (privado) para serializar/desserializar.
 */
class DiskManager {
public:
    /** Bytes ocupados pelos campos do cabeçalho no arquivo. */
    static constexpr std::size_t HEADER_DISK_SIZE = 4 + 2 + 2 + 2 + 2;

    /** Quantidade de bytes que uma Page ocupa no arquivo (cabeçalho + dados). */
    static constexpr std::size_t PAGE_DISK_SIZE = HEADER_DISK_SIZE + PAGE_SIZE;

    /**
     * Abre o arquivo informado, criando-o se não existir.
     * @throws std::runtime_error se o arquivo não puder ser aberto/criado.
     */
    explicit DiskManager(const std::string& filePath);

    DiskManager(const DiskManager&) = delete;
    DiskManager& operator=(const DiskManager&) = delete;

    /**
     * Lê a Page de identificador pageId do disco para `page`
     * (lê os bytes e os desserializa).
     * @return true em caso de sucesso; false se a Page não existir no arquivo,
     *         se o conteúdo for inválido ou ocorrer erro de I/O
     *         (nesses casos `page` permanece inalterada).
     */
    bool readPage(std::uint32_t pageId, Page& page);

    /**
     * Serializa `page` e a grava no disco, na posição determinada pelo pageId
     * da própria Page. O arquivo é estendido automaticamente se necessário.
     * @return true em caso de sucesso; false em caso de erro de I/O.
     */
    bool writePage(const Page& page);

private:
    static std::streamoff pageOffset(std::uint32_t pageId);

    /** Serializa `page` em um buffer de exatamente PAGE_DISK_SIZE bytes. */
    static std::vector<std::uint8_t> serializePage(const Page& page);

    /**
     * Reconstrói `page` a partir de um buffer de PAGE_DISK_SIZE bytes.
     * @param expectedPageId pageId esperado; se o buffer trouxer outro, ou se
     *                       os campos do cabeçalho forem incoerentes, o buffer
     *                       é considerado corrompido.
     * @return true se o buffer era válido e `page` foi atualizada; false caso
     *         contrário (`page` permanece inalterada).
     */
    static bool deserializePage(const std::vector<std::uint8_t>& bytes,
                                std::uint32_t expectedPageId, Page& page);

    std::string filePath_;
    std::fstream file_;
};

#endif //  DISK_MANAGER_HPP