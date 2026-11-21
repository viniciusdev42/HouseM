/*
 * Cada teste usa um arquivo temporário próprio, removido ao final.
 * O código de saída é 0 se todos os testes passarem e 1 caso contrário.
 *
 * Observação: a falha de gravação durante a substituição de uma Page suja não
 * é testada aqui, pois não há como provocá-la de forma portátil no DiskManager real.
 */

#include <cstdint>
#include <cstdio>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "src/Storage/PageBuffer/PageBuffer.hpp"

// ---------------------------------------------------------------------------
// Mini-framework de testes
// ---------------------------------------------------------------------------

namespace {

struct TestCase {
    const char* name;
    void (*fn)();
};

std::vector<TestCase>& registry() {
    static std::vector<TestCase> tests;
    return tests;
}

int g_failures = 0;

struct Registrar {
    Registrar(const char* name, void (*fn)()) { registry().push_back({name, fn}); }
};

} // namespace

// CHECK: registra a falha e continua. REQUIRE: registra a falha e encerra o teste.
#define CHECK(cond)                                                              \
    do {                                                                         \
        if (!(cond)) {                                                           \
            ++g_failures;                                                        \
            std::cerr << "    FALHA linha " << __LINE__ << ": " #cond "\n";      \
        }                                                                        \
    } while (0)

#define REQUIRE(cond)                                                            \
    do {                                                                         \
        if (!(cond)) {                                                           \
            ++g_failures;                                                        \
            std::cerr << "    FALHA linha " << __LINE__ << ": " #cond "\n";      \
            return;                                                              \
        }                                                                        \
    } while (0)

#define TEST(name)                                                               \
    static void name();                                                          \
    static Registrar registrar_##name(#name, name);                              \
    static void name()

// ---------------------------------------------------------------------------
// Auxiliares
// ---------------------------------------------------------------------------

namespace {

/** Arquivo temporário; removido na construção e na destruição. */
struct TempFile {
    std::string path;
    TempFile() {
        static int counter = 0;
        path = "page_buffer_test_" + std::to_string(counter++) + ".tmp";
        std::remove(path.c_str());
    }
    ~TempFile() { std::remove(path.c_str()); }
};

/**
 * Ambiente de teste. TempFile é a primeira base, portanto é destruída por
 * último: o arquivo só é removido depois de o DiskManager fechá-lo.
 */
struct Env : TempFile {
    DiskManager disk;
    PageBuffer  buffer;
    explicit Env(std::size_t capacity) : disk(path), buffer(capacity, disk) {}
};

/** Grava diretamente no disco uma Page com um único registro (slot 0). */
void seedPage(DiskManager& disk, std::uint32_t id, const std::string& text) {
    Page page;
    page.init(id);
    std::uint16_t slot = 0;
    page.insertRecord(text.data(), static_cast<std::uint16_t>(text.size()), slot);
    disk.writePage(page);
}

bool addRecord(Page* page, const std::string& text) {
    std::uint16_t slot = 0;
    return page->insertRecord(text.data(), static_cast<std::uint16_t>(text.size()), slot);
}

std::string readRecord(const Page& page, std::uint16_t slot) {
    char buf[64];
    std::uint16_t size = 0;
    if (!page.getRecord(slot, buf, sizeof(buf), size)) return "<ausente>";
    return std::string(buf, size);
}

/** Número de registros da Page gravada em disco; -1 se ela não puder ser lida. */
int diskRecords(DiskManager& disk, std::uint32_t id) {
    Page probe;
    if (!disk.readPage(id, probe)) return -1;
    return probe.getNumRecords();
}

} // namespace

// ---------------------------------------------------------------------------
// Construtor
// ---------------------------------------------------------------------------

TEST(ConstructorThrowsOnZeroCapacity) {
    TempFile file;
    DiskManager disk(file.path);
    bool thrown = false;
    try {
        PageBuffer buffer(0, disk);
    } catch (const std::invalid_argument&) {
        thrown = true;
    }
    CHECK(thrown);
}

// ---------------------------------------------------------------------------
// fetch
// ---------------------------------------------------------------------------

TEST(FetchLoadsPageContentFromDisk) {
    Env env(2);
    seedPage(env.disk, 1, "hello");

    Page* page = env.buffer.fetch(1);
    REQUIRE(page != nullptr);
    CHECK(page->getPageId() == 1);
    CHECK(page->getNumRecords() == 1);
    CHECK(readRecord(*page, 0) == "hello");
}

TEST(FetchHitReturnsSamePointer) {
    Env env(2);
    seedPage(env.disk, 1, "a");

    Page* first = env.buffer.fetch(1);
    Page* second = env.buffer.fetch(1);
    REQUIRE(first != nullptr);
    CHECK(second == first);
}

TEST(FetchReturnsNullptrWhenPageNotOnDiskAndFrameIsReleased) {
    Env env(1);
    seedPage(env.disk, 1, "a");

    CHECK(env.buffer.fetch(99) == nullptr);
    CHECK(!env.buffer.pin(99));
    CHECK(env.buffer.fetch(1) != nullptr); // o único frame foi devolvido
}

TEST(FetchReturnsNullptrWhenAllFramesPinned) {
    Env env(2);
    seedPage(env.disk, 1, "a");
    seedPage(env.disk, 2, "b");
    seedPage(env.disk, 3, "c");

    CHECK(env.buffer.fetch(1) != nullptr);
    CHECK(env.buffer.fetch(2) != nullptr);
    CHECK(env.buffer.fetch(3) == nullptr);
}

// ---------------------------------------------------------------------------
// pin / unpin
// ---------------------------------------------------------------------------

TEST(PinFailsWhenPageNotInBuffer) {
    Env env(2);
    CHECK(!env.buffer.pin(42));
}

TEST(PinIncrementsPinCount) {
    Env env(1);
    seedPage(env.disk, 1, "a");
    seedPage(env.disk, 2, "b");

    REQUIRE(env.buffer.fetch(1) != nullptr);   // pinCount = 1
    CHECK(env.buffer.pin(1));                  // pinCount = 2

    CHECK(env.buffer.unpin(1, false));         // pinCount = 1
    CHECK(env.buffer.fetch(2) == nullptr);     // ainda pinada

    CHECK(env.buffer.unpin(1, false));         // pinCount = 0
    CHECK(env.buffer.fetch(2) != nullptr);     // agora pode ser substituída
}

TEST(UnpinFailsWhenPageNotInBufferOrAlreadyUnpinned) {
    Env env(2);
    seedPage(env.disk, 1, "a");

    CHECK(!env.buffer.unpin(42, false));
    REQUIRE(env.buffer.fetch(1) != nullptr);
    CHECK(env.buffer.unpin(1, false));
    CHECK(!env.buffer.unpin(1, false));
}

// ---------------------------------------------------------------------------
// Substituição LRU
// ---------------------------------------------------------------------------

TEST(EvictsLeastRecentlyUnpinnedPage) {
    Env env(2);
    seedPage(env.disk, 1, "a");
    seedPage(env.disk, 2, "b");
    seedPage(env.disk, 3, "c");

    REQUIRE(env.buffer.fetch(1) != nullptr);
    REQUIRE(env.buffer.fetch(2) != nullptr);
    env.buffer.unpin(1, false);
    env.buffer.unpin(2, false);

    // Reutilizar a página 1 a torna a mais recente.
    REQUIRE(env.buffer.fetch(1) != nullptr);
    env.buffer.unpin(1, false);

    REQUIRE(env.buffer.fetch(3) != nullptr); // evicta a página 2
    CHECK(!env.buffer.pin(2));
    CHECK(env.buffer.pin(1));
    CHECK(env.buffer.pin(3));
}

// ---------------------------------------------------------------------------
// Gravação de páginas sujas (integração com Page e DiskManager)
// ---------------------------------------------------------------------------

TEST(DirtyPageIsWrittenToDiskOnEviction) {
    Env env(1);
    seedPage(env.disk, 1, "original");
    seedPage(env.disk, 2, "outra");

    Page* page = env.buffer.fetch(1);
    REQUIRE(page != nullptr);
    REQUIRE(addRecord(page, "novo"));
    env.buffer.unpin(1, true);

    REQUIRE(env.buffer.fetch(2) != nullptr); // evicta e grava a página 1
    CHECK(diskRecords(env.disk, 1) == 2);

    // Ao buscá-la de novo, o conteúdo gravado é recuperado.
    env.buffer.unpin(2, false);
    page = env.buffer.fetch(1);
    REQUIRE(page != nullptr);
    CHECK(readRecord(*page, 0) == "original");
    CHECK(readRecord(*page, 1) == "novo");
}

TEST(CleanPageIsNotWrittenToDiskOnEviction) {
    Env env(1);
    seedPage(env.disk, 1, "original");
    seedPage(env.disk, 2, "outra");

    Page* page = env.buffer.fetch(1);
    REQUIRE(page != nullptr);
    REQUIRE(addRecord(page, "nao gravado"));
    env.buffer.unpin(1, false);

    REQUIRE(env.buffer.fetch(2) != nullptr);
    CHECK(diskRecords(env.disk, 1) == 1); // disco mantém a versão original
}

TEST(UnpinWithFalseDoesNotClearDirtyMark) {
    Env env(1);
    seedPage(env.disk, 1, "original");
    seedPage(env.disk, 2, "outra");

    Page* page = env.buffer.fetch(1);
    REQUIRE(page != nullptr);
    REQUIRE(env.buffer.pin(1));
    REQUIRE(addRecord(page, "novo"));
    env.buffer.unpin(1, true);
    env.buffer.unpin(1, false);

    REQUIRE(env.buffer.fetch(2) != nullptr);
    CHECK(diskRecords(env.disk, 1) == 2);
}

// ---------------------------------------------------------------------------
// newPage
// ---------------------------------------------------------------------------

TEST(NewPageReturnsEmptyPinnedPageWithoutTouchingDisk) {
    Env env(1);
    seedPage(env.disk, 1, "a");
    seedPage(env.disk, 2, "b");
    seedPage(env.disk, 3, "c"); // garante que o arquivo já cobre o id 3 e o id 4 seja inédito

    Page* page = env.buffer.newPage(4);
    REQUIRE(page != nullptr);
    CHECK(page->getPageId() == 4);
    CHECK(page->getNumRecords() == 0);
    CHECK(diskRecords(env.disk, 4) == -1);      // ainda não gravada
    CHECK(env.buffer.fetch(1) == nullptr);      // pinada: não pode ser substituída
}

TEST(NewPageIsWrittenToDiskWhenEvicted) {
    Env env(1);
    seedPage(env.disk, 1, "a");

    Page* page = env.buffer.newPage(2);
    REQUIRE(page != nullptr);
    REQUIRE(addRecord(page, "criada"));
    env.buffer.unpin(2, false); // já nasce suja, mesmo com unpin(false)

    REQUIRE(env.buffer.fetch(1) != nullptr);    // evicta a página 2
    CHECK(diskRecords(env.disk, 2) == 1);

    env.buffer.unpin(1, false);
    page = env.buffer.fetch(2);
    REQUIRE(page != nullptr);
    CHECK(readRecord(*page, 0) == "criada");
}

TEST(NewPageFailsWhenIdAlreadyInBufferOrAllFramesPinned) {
    Env env(1);

    CHECK(env.buffer.newPage(1) != nullptr);
    CHECK(env.buffer.newPage(1) == nullptr); // id já presente
    CHECK(env.buffer.newPage(2) == nullptr); // sem frames disponíveis
}

// ---------------------------------------------------------------------------
// discard
// ---------------------------------------------------------------------------

TEST(DiscardReturnsTrueWhenPageNotInBuffer) {
    Env env(1);
    CHECK(env.buffer.discard(42));
}

TEST(DiscardFailsWhenPagePinned) {
    Env env(1);
    seedPage(env.disk, 1, "a");

    REQUIRE(env.buffer.fetch(1) != nullptr);
    CHECK(!env.buffer.discard(1));
    CHECK(env.buffer.pin(1)); // continua no buffer
}

TEST(DiscardDropsModificationsAndFreesFrame) {
    Env env(1);
    seedPage(env.disk, 1, "original");
    seedPage(env.disk, 2, "outra");

    Page* page = env.buffer.fetch(1);
    REQUIRE(page != nullptr);
    REQUIRE(addRecord(page, "descartado"));
    env.buffer.unpin(1, true);

    CHECK(env.buffer.discard(1));
    CHECK(!env.buffer.pin(1));
    CHECK(diskRecords(env.disk, 1) == 1);        // nada foi gravado

    // O frame foi liberado e a página volta do disco na versão original.
    REQUIRE(env.buffer.fetch(2) != nullptr);
    env.buffer.unpin(2, false);
    page = env.buffer.fetch(1);
    REQUIRE(page != nullptr);
    CHECK(page->getNumRecords() == 1);
}

TEST(DiscardOfNewPageLeavesNothingOnDisk) {
    Env env(1);

    REQUIRE(env.buffer.newPage(1) != nullptr);
    env.buffer.unpin(1, true);

    CHECK(env.buffer.discard(1));
    CHECK(diskRecords(env.disk, 1) == -1);
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main() {
    int failedTests = 0;

    for (const TestCase& test : registry()) {
        const int before = g_failures;
        try {
            test.fn();
        } catch (const std::exception& e) {
            ++g_failures;
            std::cerr << "    EXCEÇÃO: " << e.what() << "\n";
        }
        const bool ok = (g_failures == before);
        if (!ok) ++failedTests;
        std::cout << (ok ? "[ OK ] " : "[FAIL] ") << test.name << "\n";
    }

    std::cout << "\n" << (registry().size() - failedTests) << "/" << registry().size()
              << " testes passaram\n";
    return failedTests == 0 ? 0 : 1;
}
