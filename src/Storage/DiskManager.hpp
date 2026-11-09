#ifndef  DISK_MANAGER
#define  DISK_MANAGER

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>

#include "Page.hpp"

/**
 * Gerencia a leitura e escrita de Pages em um arquivo de dados em disco.
 *
 * As Pages são gravadas de forma contígua: a Page de identificador N ocupa
 * os bytes [N * PAGE_DISK_SIZE, (N + 1) * PAGE_DISK_SIZE) do arquivo.
 *
 * REQUISITO: a classe Page deve declarar `friend class DiskManager;`, pois o
 * DiskManager precisa acessar o PageHeader (que contém pageData) para
 * serializar/desserializar a Page.
 */
class DiskManager {
public:
    /** Quantidade de bytes que uma Page ocupa no arquivo (header + dados). */
    static constexpr std::size_t PAGE_DISK_SIZE = sizeof(PageHeader);

    /**
     * Abre o arquivo informado, criando-o se não existir.
     * @throws std::runtime_error se o arquivo não puder ser aberto/criado.
     */
    explicit DiskManager(const std::string& filePath);

    DiskManager(const DiskManager&) = delete;
    DiskManager& operator=(const DiskManager&) = delete;

    /**
     * Lê a Page de identificador pageId do disco para `page`.
     * @return true em caso de sucesso; false se a Page não existir no arquivo
     *         ou ocorrer erro de I/O (neste caso `page` permanece inalterada).
     */
    bool readPage(std::uint32_t pageId, Page& page);

    /**
     * Grava `page` no disco, na posição determinada por page.getPageId().
     * O arquivo é estendido automaticamente se necessário.
     * @return true em caso de sucesso; false em caso de erro de I/O.
     */
    bool writePage(const Page& page);

private:
    static std::streamoff pageOffset(std::uint32_t pageId);

    std::string filePath_;
    std::fstream file_;
};

#endif //  DISK_MANAGER
