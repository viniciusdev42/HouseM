#include "PageManager.hpp"

#include <limits>
#include <new>

Page* PageManager::allocatePage() {
    try {
        // Reaproveita um id liberado, se houver.
        if (!freePageIds_.empty()) {
            const std::uint32_t pageId = freePageIds_.back();

            auto page = std::make_unique<Page>();
            page->init(pageId);

            // Só consome o id da pilha depois que a alocação deu certo.
            freePageIds_.pop_back();
            pages_[pageId] = std::move(page);
            return pages_[pageId].get();
        }

        // Sem ids livres: cria um novo id sequencial.
        if (pages_.size() > std::numeric_limits<std::uint32_t>::max()) {
            return nullptr; // ids esgotados
        }
        const auto pageId = static_cast<std::uint32_t>(pages_.size());

        auto page = std::make_unique<Page>();
        page->init(pageId);

        pages_.push_back(std::move(page));
        return pages_.back().get();
    } catch (const std::bad_alloc&) {
        return nullptr;
    }
}

bool PageManager::freePage(std::uint32_t pageId) {
    if (pageId >= pages_.size() || !pages_[pageId]) {
        return false; // id inexistente ou Page já liberada
    }

    try {
        freePageIds_.push_back(pageId);
    } catch (const std::bad_alloc&) {
        return false; // Page preservada, estado consistente
    }

    pages_[pageId].reset();
    return true;
}
