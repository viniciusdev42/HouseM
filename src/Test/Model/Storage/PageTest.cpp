
#include "src/Main/Model/Storage/Page/Page.hpp"

#include <cstring>
#include <iostream>
#include <string>
#include <vector>

static int g_failures = 0;
static int g_tests = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            ++g_failures;                                                    \
            std::cerr << "  [FALHA] " << __FILE__ << ":" << __LINE__ << " -> " \
                      << #cond << "\n";                                      \
        }                                                                    \
    } while (0)

#define RUN(test)                                   \
    do {                                            \
        ++g_tests;                                  \
        std::cout << "- " << #test << "\n";         \
        test();                                     \
    } while (0)

// ---------------------------------------------------------------------------
// Auxiliares
// ---------------------------------------------------------------------------

static std::uint16_t insertStr(Page& page, const std::string& s) {
    std::uint16_t slot = 0xFFFF;
    bool ok = page.insertRecord(s.data(), static_cast<std::uint16_t>(s.size()), slot);
    CHECK(ok);
    return slot;
}

static std::string readStr(const Page& page, std::uint16_t slot) {
    char buf[PAGE_SIZE];
    std::uint16_t size = 0;
    if (!page.getRecord(slot, buf, sizeof(buf), size)) {
        return "<erro>";
    }
    return std::string(buf, size);
}

// ---------------------------------------------------------------------------
// Testes
// ---------------------------------------------------------------------------

void testInit() {
    Page page;
    page.init(7);
    CHECK(page.getPageId() == 7);
    CHECK(page.getNumRecords() == 0);
    CHECK(page.getFreeSpace() == PAGE_SIZE);
}

void testInsertAndGet() {
    Page page;
    page.init(1);

    std::uint16_t slot = insertStr(page, "ola");
    CHECK(readStr(page, slot) == "ola");
    CHECK(page.getNumRecords() == 1);
    // 3 bytes do registro + 4 bytes do slot
    CHECK(page.getFreeSpace() == PAGE_SIZE - 3 - 4);
}

void testInsertInvalidArgs() {
    Page page;
    std::uint16_t slot = 0;
    CHECK(!page.insertRecord(nullptr, 5, slot));
    CHECK(!page.insertRecord("abc", 0, slot));
    CHECK(page.getNumRecords() == 0);
    CHECK(page.getFreeSpace() == PAGE_SIZE);
}

void testInsertMultiple() {
    Page page;
    std::uint16_t a = insertStr(page, "primeiro");
    std::uint16_t b = insertStr(page, "segundo");
    std::uint16_t c = insertStr(page, "terceiro");

    CHECK(a != b && b != c && a != c);
    CHECK(readStr(page, a) == "primeiro");
    CHECK(readStr(page, b) == "segundo");
    CHECK(readStr(page, c) == "terceiro");
    CHECK(page.getNumRecords() == 3);
}

void testInsertNoSpace() {
    Page page;
    std::uint16_t slot = 0;

    // Registro maior que a área útil (precisa de size + 4 bytes de slot).
    std::vector<std::uint8_t> big(PAGE_SIZE, 0xAA);
    CHECK(!page.insertRecord(big.data(), PAGE_SIZE, slot));

    // Registro que ocupa exatamente todo o espaço (4092 + 4 de slot).
    std::vector<std::uint8_t> full(PAGE_SIZE - 4, 0xBB);
    CHECK(page.insertRecord(full.data(), static_cast<std::uint16_t>(full.size()), slot));
    CHECK(page.getFreeSpace() == 0);

    // Página cheia: nada mais cabe.
    CHECK(!page.insertRecord("x", 1, slot));
    CHECK(page.getNumRecords() == 1);
}

void testGetRecordInvalid() {
    Page page;
    char buf[16];
    std::uint16_t size = 0;

    // Slot inexistente.
    CHECK(!page.getRecord(0, buf, sizeof(buf), size));

    std::uint16_t slot = insertStr(page, "abcdef");

    // Buffer pequeno demais: falha, mas informa o tamanho real.
    char small[2];
    size = 0;
    CHECK(!page.getRecord(slot, small, sizeof(small), size));
    CHECK(size == 6);

    // Buffer nulo.
    CHECK(!page.getRecord(slot, nullptr, 16, size));
}

void testUpdateSameSize() {
    Page page;
    std::uint16_t slot = insertStr(page, "aaaa");
    std::uint16_t free0 = page.getFreeSpace();

    CHECK(page.updateRecord(slot, "bbbb", 4));
    CHECK(readStr(page, slot) == "bbbb");
    CHECK(page.getFreeSpace() == free0);
    CHECK(page.getNumRecords() == 1);
}

void testUpdateGrowAndShrink() {
    Page page;
    std::uint16_t a = insertStr(page, "aaa");
    std::uint16_t b = insertStr(page, "bbb");
    std::uint16_t c = insertStr(page, "ccc");
    std::uint16_t free0 = page.getFreeSpace();

    // Cresce o registro do meio; os vizinhos devem permanecer intactos.
    std::string bigger = "bbbbbbbbbb"; // 10 bytes
    CHECK(page.updateRecord(b, bigger.data(), static_cast<std::uint16_t>(bigger.size())));
    CHECK(readStr(page, a) == "aaa");
    CHECK(readStr(page, b) == bigger);
    CHECK(readStr(page, c) == "ccc");
    CHECK(page.getFreeSpace() == free0 - 7);

    // Encolhe o mesmo registro.
    CHECK(page.updateRecord(b, "b", 1));
    CHECK(readStr(page, a) == "aaa");
    CHECK(readStr(page, b) == "b");
    CHECK(readStr(page, c) == "ccc");
    CHECK(page.getFreeSpace() == free0 + 2);
}

void testUpdateInvalid() {
    Page page;
    std::uint16_t slot = insertStr(page, "abc");

    CHECK(!page.updateRecord(99, "x", 1));      // slot inexistente
    CHECK(!page.updateRecord(slot, nullptr, 1)); // dados nulos
    CHECK(!page.updateRecord(slot, "x", 0));     // tamanho zero
    CHECK(readStr(page, slot) == "abc");
}

void testUpdateNoSpacePreservesOriginal() {
    Page page;
    std::vector<std::uint8_t> full(PAGE_SIZE - 4, 0xCC);
    std::uint16_t slot = 0;
    CHECK(page.insertRecord(full.data(), static_cast<std::uint16_t>(full.size()), slot));

    std::vector<std::uint8_t> bigger(full.size() + 1, 0xDD);
    CHECK(!page.updateRecord(slot, bigger.data(), static_cast<std::uint16_t>(bigger.size())));

    // O registro original continua intacto.
    std::vector<std::uint8_t> out(PAGE_SIZE);
    std::uint16_t size = 0;
    CHECK(page.getRecord(slot, out.data(), static_cast<std::uint16_t>(out.size()), size));
    CHECK(size == full.size());
    CHECK(std::memcmp(out.data(), full.data(), size) == 0);
}

void testDelete() {
    Page page;
    std::uint16_t slot = insertStr(page, "abc");

    CHECK(page.deleteRecord(slot));
    CHECK(page.getNumRecords() == 0);
    CHECK(page.getFreeSpace() == PAGE_SIZE); // registro + slot devolvidos

    char buf[16];
    std::uint16_t size = 0;
    CHECK(!page.getRecord(slot, buf, sizeof(buf), size));
}

void testDeleteInvalid() {
    Page page;
    CHECK(!page.deleteRecord(0)); // slot inexistente

    std::uint16_t slot = insertStr(page, "abc");
    CHECK(page.deleteRecord(slot));
    CHECK(!page.deleteRecord(slot)); // já removido
}

void testDeleteMiddleKeepsOthers() {
    Page page;
    std::uint16_t a = insertStr(page, "aaa");
    std::uint16_t b = insertStr(page, "bbbb");
    std::uint16_t c = insertStr(page, "ccccc");

    CHECK(page.deleteRecord(b));
    CHECK(page.getNumRecords() == 2);
    CHECK(readStr(page, a) == "aaa");
    CHECK(readStr(page, c) == "ccccc");
}

void testSlotReuseAfterDelete() {
    Page page;
    std::uint16_t a = insertStr(page, "aaa");
    std::uint16_t b = insertStr(page, "bbb");
    (void)insertStr(page, "ccc");

    CHECK(page.deleteRecord(b));
    std::uint16_t freeBefore = page.getFreeSpace();

    // Novo registro reaproveita o slot livre (sem custo de um slot novo).
    std::uint16_t d = insertStr(page, "ddd");
    CHECK(d == b);
    CHECK(page.getFreeSpace() == freeBefore - 3);
    CHECK(readStr(page, a) == "aaa");
    CHECK(readStr(page, d) == "ddd");
}

// ---------------------------------------------------------------------------

int main() {
    RUN(testInit);
    RUN(testInsertAndGet);
    RUN(testInsertInvalidArgs);
    RUN(testInsertMultiple);
    RUN(testInsertNoSpace);
    RUN(testGetRecordInvalid);
    RUN(testUpdateSameSize);
    RUN(testUpdateGrowAndShrink);
    RUN(testUpdateInvalid);
    RUN(testUpdateNoSpacePreservesOriginal);
    RUN(testDelete);
    RUN(testDeleteInvalid);
    RUN(testDeleteMiddleKeepsOthers);
    RUN(testSlotReuseAfterDelete);

    std::cout << "\n" << g_tests << " testes, " << g_failures << " falha(s).\n";
    return g_failures == 0 ? 0 : 1;
}
