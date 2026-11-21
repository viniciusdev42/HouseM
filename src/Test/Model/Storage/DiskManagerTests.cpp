// NOTA: os testes de baixo nível (formato em disco, cabeçalho corrompido,
// preenchimento direto de pageData) precisam acessar `Page::header_`, que é
// privado. O trecho abaixo abre o acesso apenas neste arquivo. Os testes de
// integração ("page_*") usam somente a API pública da Page.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#define private public
#include "src/Main/Model/Storage/DiskManager/DiskManager.hpp"
#undef private

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Mini infraestrutura de testes
// ---------------------------------------------------------------------------

static int g_failures = 0;

#define CHECK(cond)                                                              \
    do {                                                                         \
        if (!(cond)) {                                                           \
            std::printf("    FALHOU (linha %d): %s\n", __LINE__, #cond);         \
            ++g_failures;                                                        \
        }                                                                        \
    } while (0)

// Caminho de arquivo temporário; remove o arquivo ao sair do escopo.
struct TempFile {
    std::string path;
    explicit TempFile(const std::string& name)
        : path((fs::temp_directory_path() / name).string()) {
        fs::remove(path);
    }
    ~TempFile() { fs::remove(path); }
};

// ---------------------------------------------------------------------------
// Auxiliares
// ---------------------------------------------------------------------------

// Cria uma Page válida; `seed` varia o conteúdo para distinguir Pages.
static Page makePage(std::uint32_t pageId, std::uint8_t seed) {
    Page page;
    PageHeader& h = page.header_;
    h.pageId          = pageId;
    h.numSlots        = 2;
    h.numRecords      = 2;
    h.freeSpaceOffset = 100;
    h.freeSpace       = static_cast<std::uint16_t>(PAGE_SIZE - 100 - 2 * 4);
    for (std::size_t i = 0; i < PAGE_SIZE; ++i) {
        h.pageData[i] = static_cast<std::uint8_t>(i * 7 + seed);
    }
    return page;
}

static bool samePage(const Page& a, const Page& b) {
    const PageHeader& x = a.header_;
    const PageHeader& y = b.header_;
    return x.pageId == y.pageId && x.numRecords == y.numRecords &&
           x.freeSpace == y.freeSpace && x.numSlots == y.numSlots &&
           x.freeSpaceOffset == y.freeSpaceOffset &&
           std::memcmp(x.pageData, y.pageData, PAGE_SIZE) == 0;
}

// Verifica se o registro do slot contém exatamente o texto esperado.
static bool recordEquals(const Page& page, std::uint16_t slotId, const std::string& expected) {
    char buf[256];
    std::uint16_t size = 0;
    if (!page.getRecord(slotId, buf, sizeof(buf), size)) return false;
    return size == expected.size() && std::memcmp(buf, expected.data(), size) == 0;
}

static bool insertText(Page& page, const std::string& text, std::uint16_t& slotId) {
    return page.insertRecord(text.data(), static_cast<std::uint16_t>(text.size()), slotId);
}

// ---------------------------------------------------------------------------
// Testes
// ---------------------------------------------------------------------------

static void test_constructor_creates_file() {
    TempFile tf("dm_test_create.db");
    CHECK(!fs::exists(tf.path));
    {
        DiskManager dm(tf.path);
    }
    CHECK(fs::exists(tf.path));
    CHECK(fs::file_size(tf.path) == 0);
}

static void test_constructor_throws_on_invalid_path() {
    bool threw = false;
    try {
        DiskManager dm((fs::temp_directory_path() / "dir_inexistente_xyz" / "a.db").string());
    } catch (const std::runtime_error&) {
        threw = true;
    }
    CHECK(threw);
}

static void test_write_then_read_roundtrip() {
    TempFile tf("dm_test_roundtrip.db");
    DiskManager dm(tf.path);

    const Page original = makePage(0, 1);
    CHECK(dm.writePage(original));

    Page loaded;
    CHECK(dm.readPage(0, loaded));
    CHECK(samePage(original, loaded));
}

static void test_write_uses_page_offset_and_extends_file() {
    TempFile tf("dm_test_offset.db");
    DiskManager dm(tf.path);

    CHECK(dm.writePage(makePage(3, 9)));
    CHECK(fs::file_size(tf.path) == 4 * DiskManager::PAGE_DISK_SIZE);

    Page loaded;
    CHECK(dm.readPage(3, loaded));
    CHECK(samePage(makePage(3, 9), loaded));
}

static void test_overwrite_only_changes_target_page() {
    TempFile tf("dm_test_overwrite.db");
    DiskManager dm(tf.path);

    CHECK(dm.writePage(makePage(0, 1)));
    CHECK(dm.writePage(makePage(1, 2)));
    CHECK(dm.writePage(makePage(0, 50)));  // sobrescreve a Page 0

    Page p0, p1;
    CHECK(dm.readPage(0, p0));
    CHECK(dm.readPage(1, p1));
    CHECK(samePage(makePage(0, 50), p0));
    CHECK(samePage(makePage(1, 2), p1));
    CHECK(fs::file_size(tf.path) == 2 * DiskManager::PAGE_DISK_SIZE);
}

static void test_data_persists_across_instances() {
    TempFile tf("dm_test_persist.db");
    {
        DiskManager dm(tf.path);
        CHECK(dm.writePage(makePage(0, 7)));
    }
    DiskManager dm2(tf.path);
    Page loaded;
    CHECK(dm2.readPage(0, loaded));
    CHECK(samePage(makePage(0, 7), loaded));
}

static void test_read_nonexistent_page_fails_and_keeps_page() {
    TempFile tf("dm_test_missing.db");
    DiskManager dm(tf.path);

    Page page = makePage(5, 3);
    const Page before = page;

    CHECK(!dm.readPage(0, page));  // arquivo vazio
    CHECK(dm.writePage(makePage(0, 1)));
    CHECK(!dm.readPage(1, page));  // além do fim do arquivo
    CHECK(samePage(before, page));
}

static void test_read_hole_in_file_fails() {
    TempFile tf("dm_test_hole.db");
    DiskManager dm(tf.path);

    CHECK(dm.writePage(makePage(2, 1)));  // Pages 0 e 1 viram "buraco" (zeros)

    Page page;
    CHECK(!dm.readPage(1, page));  // pageId lido (0) != esperado (1)
    CHECK(dm.readPage(2, page));
}

static void test_read_corrupted_header_fails_and_keeps_page() {
    TempFile tf("dm_test_corrupt.db");
    DiskManager dm(tf.path);

    Page bad = makePage(0, 1);
    bad.header_.numRecords = 10;  // numRecords > numSlots: incoerente
    CHECK(dm.writePage(bad));     // writePage não valida; grava como está

    Page page = makePage(0, 99);
    const Page before = page;
    CHECK(!dm.readPage(0, page));
    CHECK(samePage(before, page));
}

static void test_disk_format_is_little_endian() {
    TempFile tf("dm_test_endian.db");
    {
        DiskManager dm(tf.path);
        Page page = makePage(0, 1);
        page.header_.numRecords = 0x0102;
        CHECK(dm.writePage(page));
    }

    std::ifstream in(tf.path, std::ios::binary);
    std::vector<std::uint8_t> raw(DiskManager::HEADER_DISK_SIZE);
    in.read(reinterpret_cast<char*>(raw.data()), static_cast<std::streamsize>(raw.size()));
    CHECK(in.gcount() == static_cast<std::streamsize>(raw.size()));

    CHECK(raw[4] == 0x02);  // numRecords: byte menos significativo primeiro
    CHECK(raw[5] == 0x01);
}

// ---------------------------------------------------------------------------
// Integração com Page (somente API pública da Page)
// ---------------------------------------------------------------------------

static void test_page_empty_roundtrip() {
    TempFile tf("dm_test_page_empty.db");
    DiskManager dm(tf.path);

    Page page;
    page.init(2);
    CHECK(dm.writePage(page));

    Page loaded;
    CHECK(dm.readPage(2, loaded));
    CHECK(loaded.getPageId() == 2);
    CHECK(loaded.getNumRecords() == 0);
    CHECK(loaded.getFreeSpace() == PAGE_SIZE);
}

static void test_page_records_roundtrip() {
    TempFile tf("dm_test_page_records.db");
    DiskManager dm(tf.path);

    Page page;
    page.init(4);
    std::uint16_t s0 = 0, s1 = 0, s2 = 0;
    CHECK(insertText(page, "alpha", s0));
    CHECK(insertText(page, "beta-beta", s1));
    CHECK(insertText(page, "c", s2));
    CHECK(dm.writePage(page));

    Page loaded;
    CHECK(dm.readPage(4, loaded));
    CHECK(loaded.getPageId() == page.getPageId());
    CHECK(loaded.getNumRecords() == 3);
    CHECK(loaded.getFreeSpace() == page.getFreeSpace());
    CHECK(recordEquals(loaded, s0, "alpha"));
    CHECK(recordEquals(loaded, s1, "beta-beta"));
    CHECK(recordEquals(loaded, s2, "c"));
}

static void test_page_modify_after_reload_and_persist() {
    TempFile tf("dm_test_page_modify.db");
    DiskManager dm(tf.path);

    Page page;
    page.init(1);
    std::uint16_t s0 = 0, s1 = 0;
    CHECK(insertText(page, "alpha", s0));
    CHECK(insertText(page, "beta", s1));
    CHECK(dm.writePage(page));

    // Lê, altera (delete + update + insert reaproveitando o slot) e grava de novo.
    Page loaded;
    CHECK(dm.readPage(1, loaded));
    CHECK(loaded.deleteRecord(s1));
    const std::string longer = "alpha-alongado";
    CHECK(loaded.updateRecord(s0, longer.data(), static_cast<std::uint16_t>(longer.size())));
    std::uint16_t s2 = 0;
    CHECK(insertText(loaded, "gama", s2));
    CHECK(s2 == s1);  // slot livre reutilizado
    CHECK(dm.writePage(loaded));

    Page again;
    CHECK(dm.readPage(1, again));
    CHECK(again.getNumRecords() == 2);
    CHECK(again.getFreeSpace() == loaded.getFreeSpace());
    CHECK(recordEquals(again, s0, longer));
    CHECK(recordEquals(again, s2, "gama"));
}

static void test_page_multiple_pages_keep_own_records() {
    TempFile tf("dm_test_page_multi.db");
    DiskManager dm(tf.path);

    Page a, b;
    a.init(0);
    b.init(1);
    std::uint16_t sa = 0, sb = 0;
    CHECK(insertText(a, "pagina-A", sa));
    CHECK(insertText(b, "pagina-B", sb));
    CHECK(dm.writePage(a));
    CHECK(dm.writePage(b));

    Page la, lb;
    CHECK(dm.readPage(0, la));
    CHECK(dm.readPage(1, lb));
    CHECK(recordEquals(la, sa, "pagina-A"));
    CHECK(recordEquals(lb, sb, "pagina-B"));
}

// ---------------------------------------------------------------------------

int main() {
    struct { const char* name; void (*fn)(); } tests[] = {
        {"constructor cria arquivo",               test_constructor_creates_file},
        {"constructor lanca excecao",              test_constructor_throws_on_invalid_path},
        {"write + read (ida e volta)",             test_write_then_read_roundtrip},
        {"offset e extensao do arquivo",           test_write_uses_page_offset_and_extends_file},
        {"sobrescrita afeta so a Page alvo",       test_overwrite_only_changes_target_page},
        {"persistencia entre instancias",          test_data_persists_across_instances},
        {"read de Page inexistente",               test_read_nonexistent_page_fails_and_keeps_page},
        {"read de buraco no arquivo",              test_read_hole_in_file_fails},
        {"read de cabecalho corrompido",           test_read_corrupted_header_fails_and_keeps_page},
        {"formato em disco little-endian",         test_disk_format_is_little_endian},
        {"Page vazia (ida e volta)",               test_page_empty_roundtrip},
        {"Page com registros (ida e volta)",       test_page_records_roundtrip},
        {"Page alterada apos reload",              test_page_modify_after_reload_and_persist},
        {"varias Pages mantem seus registros",     test_page_multiple_pages_keep_own_records},
    };

    for (const auto& t : tests) {
        const int before = g_failures;
        std::printf("[ RUN  ] %s\n", t.name);
        t.fn();
        std::printf("[ %s ] %s\n", g_failures == before ? " OK " : "FAIL", t.name);
    }

    std::printf("\n%s (%d falha(s))\n", g_failures == 0 ? "TODOS OS TESTES PASSARAM" : "HA FALHAS",
                g_failures);
    return g_failures == 0 ? 0 : 1;
}
