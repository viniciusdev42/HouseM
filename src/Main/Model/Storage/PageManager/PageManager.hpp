#ifndef PAGE_MANAGER_HPP
#define PAGE_MANAGER_HPP

#include <cstdint>
#include <vector>

#include "src/Main/Model/Storage/Page/Page.hpp"
#include "src/Main/Model/Storage/Page/PageBuffer.hpp"

/**
 * Gerenciador de Pages.
 *
 * Responsável por alocar/liberar pageIds e por obter Pages por meio do
 * PageBuffer, que é quem mantém as Pages em memória e as persiste por meio de DiskManager.
 */
class PageManager {
public:
    explicit PageManager(PageBuffer& pageBuffer) noexcept;

    PageManager(const PageManager&) = delete;
    PageManager& operator=(const PageManager&) = delete;

    /**
     * Aloca uma Page nova e vazia diretamente no PageBuffer (já inicializada
     * com seu pageId e marcada como suja). Reaproveita o id de uma Page
     * previamente liberada, se houver.
     *
     * A Page retornada permanece pinada; o chamador deve liberá-la com
     * PageBuffer::unpin() quando terminar de utilizá-la.
     *
     * @return Ponteiro para a Page; nullptr se não for possível alocar
     *         (falta de memória, esgotamento de ids, ou nenhum frame
     *         disponível no buffer).
     */
    Page* allocatePage();

    /** Obtém uma Page pelo identificador através do PageBuffer (retorna pinada). */
    Page* getPage(std::uint32_t pageId);

    /**
     * Devolve o pageId à free list. Se a Page ainda estiver no buffer, ela é
     * descartada sem ser gravada em disco.
     *
     * @return true em caso de sucesso; false se o pageId não existir, a Page
     *         já estiver liberada ou ainda estiver pinada.
     */
    bool freePage(std::uint32_t pageId);

private:
    PageBuffer& pageBuffer_;

    /** Indexado por pageId. true = id alocado; false = id liberado. */
    std::vector<bool> allocated_;

    /** Pilha de pageIds liberados, disponíveis para reaproveitamento. */
    std::vector<std::uint32_t> freePageIds_;
};

#endif // PAGE_MANAGER_HPP