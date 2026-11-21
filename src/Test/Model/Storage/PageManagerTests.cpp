#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>

#include "src/Main/Model/Storage/PageManager/PageManager.hpp"

// ---------------------------------------------------------------------------
// Mini-infraestrutura de testes
// ---------------------------------------------------------------------------
namespace {

int g_failures = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            ++g_failures;                                                    \
            std::cerr << "    [FALHA] " << __FILE__ << ":" << __LINE__       \
                      << "  CHECK(" #cond ")\n";                             \
        }                                                                    \
    } while (false)

#define RUN_TEST(fn)                                                         \
    do {                                                                     \
        const int before = g_failures;                                       \
        fn();                                                                \
        std::cout << (g_failures == before ? "[OK]    " : "[FALHOU] ")        \
                  << #fn << "\n";                                            \
    } while (false)

// ---------------------------------------------------------------------------
// Fixture: DiskManager + PageBuffer + PageManager reais
// ---------------------------------------------------------------------------
// O DiskManager reabre arquivos existentes (não os trunca) e lê as Pages que
// já estão nele. Por isso o arquivo é apagado antes e depois de cada teste:
// assim nenhum teste enxerga Pages gravadas por outro. Por ser a classe base,
// TempDbFile é construída antes e destruída depois do DiskManager (que já terá
// fechado o arquivo quando ele for removido).
struct TempDbFile {
    static constexpr const char* PATH = "page_manager_test.db";
    TempDbFile()  { std::remove(PATH); }
    ~TempDbFile() { std::remove(PATH); }
};

struct Fixture : private TempDbFile {
    DiskManager diskManager{PATH};
    PageBuffer  pageBuffer;
    PageManager manager;

    explicit Fixture(std::size_t capacity = 8)
        : pageBuffer(capacity, diskManager), manager(pageBuffer) {}
};

bool unpin(Fixture& f, std::uint32_t pageId, bool dirty = true) {
    return f.pageBuffer.unpin(pageId, dirty);
}

bool insertText(Page* page, const char* text, std::uint16_t& slotId) {
    return page->insertRecord(text, static_cast<std::uint16_t>(std::strlen(text)), slotId);
}

bool recordEquals(const Page* page, std::uint16_t slotId, const char* text) {
    char buffer[64] = {};
    std::uint16_t size = 0;
    if (!page->getRecord(slotId, buffer, sizeof(buffer), size)) {
        return false;
    }
    return size == std::strlen(text) && std::memcmp(buffer, text, size) == 0;
}

// ---------------------------------------------------------------------------
// allocatePage
// ---------------------------------------------------------------------------

void test_allocatePage_retornaPageNovaEVazia() {
    Fixture f;
    Page* page = f.manager.allocatePage();
    CHECK(page != nullptr);
    if (page == nullptr) return;

    CHECK(page->getPageId() == 0);
    CHECK(page->getNumRecords() == 0);
    CHECK(page->getFreeSpace() == PAGE_SIZE);
}

void test_allocatePage_idsSequenciaisEPagesDistintas() {
    Fixture f;
    Page* a = f.manager.allocatePage();
    Page* b = f.manager.allocatePage();
    CHECK(a != nullptr && b != nullptr);
    if (a == nullptr || b == nullptr) return;

    CHECK(a != b);
    CHECK(a->getPageId() == 0);
    CHECK(b->getPageId() == 1);
}

void test_allocatePage_pageRetornadaFicaPinada() {
    Fixture f;
    CHECK(f.manager.allocatePage() != nullptr); // pinCount = 1
    CHECK(unpin(f, 0));                          // 1 -> 0
    CHECK(!unpin(f, 0));                         // já estava em 0
}

void test_allocatePage_reaproveitaIdLiberadoComPageVazia() {
    Fixture f;
    Page* p0 = f.manager.allocatePage();
    Page* p1 = f.manager.allocatePage();
    CHECK(p0 != nullptr && p1 != nullptr);
    if (p0 == nullptr || p1 == nullptr) return;

    std::uint16_t slot = 0;
    CHECK(insertText(p0, "lixo", slot));
    CHECK(unpin(f, 0));
    CHECK(unpin(f, 1));
    CHECK(f.manager.freePage(0));

    // Reutiliza o id 0, e a Page volta vazia.
    Page* reused = f.manager.allocatePage();
    CHECK(reused != nullptr);
    if (reused == nullptr) return;
    CHECK(reused->getPageId() == 0);
    CHECK(reused->getNumRecords() == 0);

    // Sem ids livres: volta a criar ids novos (o próximo é 2).
    Page* next = f.manager.allocatePage();
    CHECK(next != nullptr);
    if (next != nullptr) CHECK(next->getPageId() == 2);
}

void test_allocatePage_bufferCheioRetornaNullptrSemConsumirId() {
    Fixture f(2);
    CHECK(f.manager.allocatePage() != nullptr); // id 0, pinada
    CHECK(f.manager.allocatePage() != nullptr); // id 1, pinada

    // Todos os frames pinados: falha, e o id 2 deve ser desfeito.
    CHECK(f.manager.allocatePage() == nullptr);

    CHECK(unpin(f, 0));
    Page* page = f.manager.allocatePage();      // evicta a Page 0
    CHECK(page != nullptr);
    if (page != nullptr) CHECK(page->getPageId() == 2);
}

// ---------------------------------------------------------------------------
// getPage
// ---------------------------------------------------------------------------

void test_getPage_retornaMesmaPageEPinaNovamente() {
    Fixture f;
    Page* allocated = f.manager.allocatePage(); // pinCount = 1
    CHECK(allocated != nullptr);

    Page* fetched = f.manager.getPage(0);       // pinCount = 2
    CHECK(fetched == allocated);

    CHECK(unpin(f, 0));
    CHECK(!f.manager.freePage(0));              // ainda pinada (1)
    CHECK(unpin(f, 0));
    CHECK(f.manager.freePage(0));
}

void test_getPage_preservaConteudoDaPage() {
    Fixture f;
    Page* page = f.manager.allocatePage();
    CHECK(page != nullptr);
    if (page == nullptr) return;

    std::uint16_t slot = 0;
    CHECK(insertText(page, "ola", slot));
    CHECK(unpin(f, 0));

    Page* fetched = f.manager.getPage(0);
    CHECK(fetched != nullptr);
    if (fetched != nullptr) CHECK(recordEquals(fetched, slot, "ola"));
}

void test_getPage_leDoDiscoAposEvicao() {
    Fixture f(2);
    Page* p0 = f.manager.allocatePage();
    CHECK(p0 != nullptr);
    if (p0 == nullptr) return;

    std::uint16_t slot = 0;
    CHECK(insertText(p0, "persistido", slot));
    CHECK(unpin(f, 0));

    // Duas novas Pages forçam a evicção (e gravação) da Page 0.
    CHECK(f.manager.allocatePage() != nullptr); // id 1
    CHECK(unpin(f, 1));
    CHECK(f.manager.allocatePage() != nullptr); // id 2
    CHECK(unpin(f, 2));

    Page* fetched = f.manager.getPage(0);       // lida do disco
    CHECK(fetched != nullptr);
    if (fetched == nullptr) return;
    CHECK(fetched->getPageId() == 0);
    CHECK(fetched->getNumRecords() == 1);
    CHECK(recordEquals(fetched, slot, "persistido"));
}

void test_getPage_idInexistenteRetornaNullptr() {
    Fixture f;
    // Arquivo vazio: DiskManager::readPage falha e o PageBuffer devolve nullptr.
    CHECK(f.manager.getPage(0) == nullptr);
    CHECK(f.manager.getPage(99) == nullptr);

    // A falha não deixa o buffer em estado inconsistente.
    CHECK(f.manager.allocatePage() != nullptr);
}

// ---------------------------------------------------------------------------
// freePage
// ---------------------------------------------------------------------------

void test_freePage_idInexistenteRetornaFalse() {
    Fixture f;
    CHECK(!f.manager.freePage(0));              // nada alocado ainda
    CHECK(!f.manager.freePage(12345));
}

void test_freePage_descartaPageDoBuffer() {
    Fixture f;
    CHECK(f.manager.allocatePage() != nullptr);
    CHECK(unpin(f, 0));
    CHECK(f.manager.freePage(0));
    CHECK(!f.pageBuffer.pin(0));                // não está mais no buffer
}

void test_freePage_liberacaoDuplaRetornaFalse() {
    Fixture f;
    CHECK(f.manager.allocatePage() != nullptr);
    CHECK(unpin(f, 0));
    CHECK(f.manager.freePage(0));
    CHECK(!f.manager.freePage(0));
}

void test_freePage_pageAindaPinadaRetornaFalse() {
    Fixture f;
    CHECK(f.manager.allocatePage() != nullptr); // continua pinada
    CHECK(!f.manager.freePage(0));              // falha e continua alocada

    CHECK(unpin(f, 0));
    CHECK(f.manager.freePage(0));               // agora consegue
}

} // namespace

int main() {
    RUN_TEST(test_allocatePage_retornaPageNovaEVazia);
    RUN_TEST(test_allocatePage_idsSequenciaisEPagesDistintas);
    RUN_TEST(test_allocatePage_pageRetornadaFicaPinada);
    RUN_TEST(test_allocatePage_reaproveitaIdLiberadoComPageVazia);
    RUN_TEST(test_allocatePage_bufferCheioRetornaNullptrSemConsumirId);
    RUN_TEST(test_getPage_retornaMesmaPageEPinaNovamente);
    RUN_TEST(test_getPage_preservaConteudoDaPage);
    RUN_TEST(test_getPage_leDoDiscoAposEvicao);
    RUN_TEST(test_getPage_idInexistenteRetornaNullptr);
    RUN_TEST(test_freePage_idInexistenteRetornaFalse);
    RUN_TEST(test_freePage_descartaPageDoBuffer);
    RUN_TEST(test_freePage_liberacaoDuplaRetornaFalse);
    RUN_TEST(test_freePage_pageAindaPinadaRetornaFalse);

    if (g_failures == 0) {
        std::cout << "\nTodos os testes passaram.\n";
    } else {
        std::cout << "\n" << g_failures << " verificacao(oes) falharam.\n";
    }
    return g_failures;
}
