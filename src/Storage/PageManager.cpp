#include "PageManager.hpp"

#include <stdexcept>
#include <utility>

namespace PageManager {

PageId PageManager::createPage(std::string title, std::string content) {
    if (title.empty()) {
        throw std::invalid_argument("PageManager::createPage: o titulo da pagina nao pode ser vazio");
    }

    const PageId id = nextId_++;
    pages_.emplace(id, Page{id, std::move(title), std::move(content)});
    return id;
}

bool PageManager::removePage(PageId pageId) {
    return pages_.erase(pageId) > 0;
}

void PageManager::clearPages() {
    pages_.clear();
}

const Page* PageManager::getPage(PageId pageId) const {
    const auto it = pages_.find(pageId);
    return it != pages_.end() ? &it->second : nullptr;
}

std::vector<Page> PageManager::getAllPages() const {
    std::vector<Page> result;
    result.reserve(pages_.size());
    for (const auto& entry : pages_) {
        result.push_back(entry.second);
    }
    return result;
}

bool PageManager::hasPage(PageId pageId) const {
    return pages_.find(pageId) != pages_.end();
}

bool PageManager::validatePage(PageId pageId) const {
    const Page* page = getPage(pageId);
    if (page == nullptr) {
        return false;
    }
    return page->id == pageId && !page->title.empty();
}

}  // namespace PageManager
