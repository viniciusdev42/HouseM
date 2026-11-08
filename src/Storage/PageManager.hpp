#ifndef PAGE_MANAGER
#define PAGE_MANAGER

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace PageManager {

using PageId = std::uint32_t;

struct Page {
    PageId id{0};
    std::string title;
    std::string content;
};

// Gerencia o ciclo de vida de páginas em memória.
// Não é thread-safe: sincronize externamente se houver acesso concorrente.
class PageManager {
public:
    // Cria uma página e retorna o id gerado (sempre > 0).
    // Lança std::invalid_argument se o título estiver vazio.
    PageId createPage(std::string title, std::string content = {});

    // Remove a página. Retorna true se ela existia e foi removida.
    bool removePage(PageId pageId);

    // Remove todas as páginas. Os ids já usados não são reaproveitados.
    void clearPages();

    // Retorna ponteiro somente leitura para a página, ou nullptr se não existir.
    // O ponteiro é invalidado por removePage/clearPages da mesma página.
    const Page* getPage(PageId pageId) const;

    // Retorna cópia de todas as páginas, ordenadas por id.
    std::vector<Page> getAllPages() const;

    bool hasPage(PageId pageId) const;

    // Retorna true se a página existe e está consistente
    // (id armazenado coincide com a chave e o título não é vazio).
    bool validatePage(PageId pageId) const;

private:
    PageId nextId_{1};
    std::map<PageId, Page> pages_;
};

}  // namespace PageManager

#endif 
