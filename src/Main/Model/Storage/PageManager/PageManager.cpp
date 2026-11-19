#include "PageManager.hpp"

#include <limits>
#include <new>

PageManager::PageManager(PageBuffer& pageBuffer) noexcept
    : pageBuffer_(pageBuffer) {
}

Page* PageManager::allocatePage() {
    try {
        // Reaproveita um id liberado, se houver.
        if (!freePageIds_.empty()) {
            const std::uint32_t pageId = freePageIds_.back();

            Page* page = pageBuffer_.newPage(pageId);
            if (page == nullptr) {
                return nullptr; // o id continua na pilha
            }

            freePageIds_.pop_back();
            allocated_[pageId] = true;
            return page;
        }

        // Sem ids livres: cria um novo id sequencial.
        if (allocated_.size() > std::numeric_limits<std::uint32_t>::max()) {
            return nullptr; // ids esgotados
        }
        const auto pageId = static_cast<std::uint32_t>(allocated_.size());

        allocated_.push_back(false); // reserva o slot antes de tocar no buffer
        Page* page = pageBuffer_.newPage(pageId);
        if (page == nullptr) {
            allocated_.pop_back();   // desfaz a reserva
            return nullptr;
        }

        allocated_[pageId] = true;
        return page;
    } catch (const std::bad_alloc&) {
        return nullptr;
    }
}

Page* PageManager::getPage(std::uint32_t pageId) {
    return pageBuffer_.fetch(pageId);
}

bool PageManager::freePage(std::uint32_t pageId) {
    if (pageId >= allocated_.size() || !allocated_[pageId]) {
        return false; // id inexistente ou Page já liberada
    }

    // Reserva espaço antes de descartar: o push_back abaixo não pode falhar,
    // então nunca perdemos a Page do buffer sem registrar o id como livre.
    try {
        freePageIds_.reserve(freePageIds_.size() + 1);
    } catch (const std::bad_alloc&) {
        return false;
    }

    if (!pageBuffer_.discard(pageId)) {
        return false; // Page ainda pinada: continua alocada
    }

    freePageIds_.push_back(pageId);
    allocated_[pageId] = false;
    return true;
}