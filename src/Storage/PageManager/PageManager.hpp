#ifndef PAGE_MANAGER
#define PAGE_MANAGER

#include <cstdint>
#include <memory>
#include <vector>

#include "src/Storage/Page/Page.hpp"

/**
 * Gerenciador de Pages em memória.
 *
 * Responsável apenas por alocar e liberar Pages. Cada Page recebe um pageId
 * único; ids de Pages liberadas são reaproveitados em alocações futuras.
 *
 * O PageManager é o dono das Pages: ponteiros devolvidos por allocatePage()
 * tornam-se inválidos após freePage() do respectivo pageId ou após a
 * destruição do PageManager.
 */
class PageManager {
public:
    PageManager() = default;

    PageManager(const PageManager&) = delete;
    PageManager& operator=(const PageManager&) = delete;

    /**
     * Aloca uma Page nova e vazia (já inicializada com seu pageId).
     * Reaproveita o id de uma Page previamente liberada, se houver.
     * @return Ponteiro para a Page alocada; nullptr se não for possível
     *         alocar (falta de memória ou esgotamento de ids).
     */
    Page* allocatePage();

    /**
     * Libera a Page identificada por pageId e disponibiliza o id para reuso.
     * @return true em caso de sucesso; false se o pageId não existir ou a
     *         Page já estiver liberada.
     */
    bool freePage(std::uint32_t pageId);

private:
    /** Indexado por pageId. Entrada nula indica Page liberada. */
    std::vector<std::unique_ptr<Page>> pages_;

    /** Pilha de pageIds liberados, disponíveis para reaproveitamento. */
    std::vector<std::uint32_t> freePageIds_;
};

#endif // PAGE_MANAGER
