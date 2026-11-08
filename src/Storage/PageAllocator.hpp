#ifndef PAGE_ALLOCATOR
#define PAGE_ALLOCATOR

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

/**
 * Alocador de páginas de tamanho fixo.
 *
 * Gerencia um pool contínuo de memória dividido em `pageCount` páginas de
 * `pageSize` bytes. Cada página é identificada por um `PageId` (índice 0..N-1).
 *
 * - allocatePage()        -> reserva a menor página livre e retorna seu pageId.
 * - allocatePage(pageId)  -> reserva uma página específica.
 * - deallocatePage(pageId)-> devolve a página ao pool.
 *
 * Todas as operações são thread-safe.
 */
class PageAllocator {
public:
    using PageId = std::uint32_t;

    PageAllocator(std::size_t pageCount, std::size_t pageSize)
        : pageCount_(pageCount),
          pageSize_(pageSize),
          memory_() {
        if (pageCount_ == 0) {
            throw std::invalid_argument("PageAllocator: pageCount deve ser maior que zero");
        }
        if (pageSize_ == 0) {
            throw std::invalid_argument("PageAllocator: pageSize deve ser maior que zero");
        }
        // Evita overflow na multiplicação antes de reservar a memória.
        if (pageCount_ > SIZE_MAX / pageSize_) {
            throw std::length_error("PageAllocator: tamanho total do pool excede o limite");
        }
        memory_.resize(pageCount_ * pageSize_);

        // Conjunto ordenado: garante alocação determinística (menor id primeiro)
        // e permite alocar/remover um id específico em O(log n).
        for (std::size_t id = 0; id < pageCount_; ++id) {
            freePages_.insert(static_cast<PageId>(id));
        }
    }

    // O pool possui memória e mutex, portanto não deve ser copiado.
    PageAllocator(const PageAllocator&) = delete;
    PageAllocator& operator=(const PageAllocator&) = delete;

    /**
     * Aloca a menor página livre.
     * @return pageId da página alocada.
     * @throws std::runtime_error se não houver páginas livres.
     */
    PageId allocatePage() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (freePages_.empty()) {
            throw std::runtime_error("PageAllocator: nenhuma página livre disponível");
        }
        const auto it = freePages_.begin();
        const PageId pageId = *it;
        freePages_.erase(it);
        return pageId;
    }

    /**
     * Aloca uma página específica.
     * @throws std::out_of_range se o pageId for inválido.
     * @throws std::logic_error  se a página já estiver alocada.
     */
    PageId allocatePage(PageId pageId) {
        std::lock_guard<std::mutex> lock(mutex_);
        validateId(pageId);
        if (freePages_.erase(pageId) == 0) {
            throw std::logic_error("PageAllocator: página " + std::to_string(pageId) +
                                   " já está alocada");
        }
        return pageId;
    }

    /**
     * Libera uma página previamente alocada.
     * @throws std::out_of_range se o pageId for inválido.
     * @throws std::logic_error  se a página já estiver livre (double free).
     */
    void deallocatePage(PageId pageId) {
        std::lock_guard<std::mutex> lock(mutex_);
        validateId(pageId);
        if (!freePages_.insert(pageId).second) {
            throw std::logic_error("PageAllocator: página " + std::to_string(pageId) +
                                   " já está livre (double free)");
        }
    }

    /**
     * Retorna o ponteiro para o início da página, para leitura/escrita.
     * @throws std::out_of_range se o pageId for inválido.
     * @throws std::logic_error  se a página não estiver alocada.
     */
    std::byte* pageData(PageId pageId) {
        std::lock_guard<std::mutex> lock(mutex_);
        validateId(pageId);
        if (freePages_.count(pageId) != 0) {
            throw std::logic_error("PageAllocator: página " + std::to_string(pageId) +
                                   " não está alocada");
        }
        return memory_.data() + static_cast<std::size_t>(pageId) * pageSize_;
    }

    bool isAllocated(PageId pageId) const {
        std::lock_guard<std::mutex> lock(mutex_);
        validateId(pageId);
        return freePages_.count(pageId) == 0;
    }

    std::size_t pageSize() const noexcept { return pageSize_; }
    std::size_t pageCount() const noexcept { return pageCount_; }

    std::size_t freePageCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return freePages_.size();
    }

private:
    // Deve ser chamado com o mutex já adquirido (apenas lê membros imutáveis).
    void validateId(PageId pageId) const {
        if (pageId >= pageCount_) {
            throw std::out_of_range("PageAllocator: pageId " + std::to_string(pageId) +
                                    " fora do intervalo [0, " +
                                    std::to_string(pageCount_) + ")");
        }
    }

    const std::size_t pageCount_;
    const std::size_t pageSize_;
    std::vector<std::byte> memory_;
    std::set<PageId> freePages_;
    mutable std::mutex mutex_;
};

#endif // PAGE_ALLOCATOR
