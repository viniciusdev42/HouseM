#include "src/Storage/Serializer/Serializer.hpp"

#include <cstdint>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <vector>

// ---------------------------------------------------------------------------
// Mini framework de testes.
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

#define RUN(test)                                                            \
    do {                                                                     \
        const int before = g_failures;                                       \
        test();                                                              \
        std::cout << (g_failures == before ? "[ OK ] " : "[FAIL] ") << #test \
                  << "\n";                                                   \
    } while (false)

// ---------------------------------------------------------------------------
// Auxiliares.
// ---------------------------------------------------------------------------

using Bytes = std::vector<std::uint8_t>;
using Ids = std::vector<std::uint32_t>;

void putU16(Bytes& out, std::uint16_t v) {
    out.push_back(static_cast<std::uint8_t>(v & 0xFF));
    out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
}

void putU32(Bytes& out, std::uint32_t v) {
    for (int i = 0; i < 4; ++i) {
        out.push_back(static_cast<std::uint8_t>((v >> (8 * i)) & 0xFF));
    }
}

// Monta manualmente um registro no formato documentado em Serializer.hpp.
Bytes makeRecord(std::uint8_t leaf, std::uint32_t minDegree,
                 const std::vector<std::int32_t>& keys, const Ids& children) {
    Bytes b;
    b.push_back(leaf);
    putU32(b, minDegree);
    putU16(b, static_cast<std::uint16_t>(keys.size()));
    for (std::int32_t k : keys) putU32(b, static_cast<std::uint32_t>(k));
    putU16(b, static_cast<std::uint16_t>(children.size()));
    for (std::uint32_t c : children) putU32(b, c);
    return b;
}

// Insere bytes brutos na Page, contornando o Serializer.
bool insertRaw(Page& page, const Bytes& bytes, std::uint16_t& slotId) {
    return page.insertRecord(bytes.data(),
                             static_cast<std::uint16_t>(bytes.size()), slotId);
}

// Lê de volta os bytes brutos de um slot.
Bytes readRaw(const Page& page, std::uint16_t slotId) {
    Bytes buffer(PAGE_SIZE);
    std::uint16_t size = 0;
    if (!page.getRecord(slotId, buffer.data(),
                        static_cast<std::uint16_t>(buffer.size()), size)) {
        return {};
    }
    buffer.resize(size);
    return buffer;
}

// Compara chaves de dois nós.
bool sameKeys(const BTreeNode& a, const BTreeNode& b) {
    if (a.keyCount() != b.keyCount()) return false;
    for (std::size_t i = 0; i < a.keyCount(); ++i) {
        if (a.getKey(i) != b.getKey(i)) return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// serialize()
// ---------------------------------------------------------------------------

// O registro gravado deve seguir exatamente o formato documentado.
void test_serialize_writesExpectedFormat() {
    BTreeNode node(3, true);
    node.insertKey(10);
    node.insertKey(-5); // o nó mantém as chaves ordenadas: -5, 10

    Page page;
    std::uint16_t slot = 0;
    CHECK(Serializer::serialize(node, {}, page, slot));

    CHECK(readRaw(page, slot) == makeRecord(1, 3, {-5, 10}, {}));
}

// Sucesso consome espaço e registra exatamente um registro na Page.
void test_serialize_storesOneRecordInPage() {
    BTreeNode node(2, true);
    node.insertKey(1);

    Page page;
    const std::uint16_t freeBefore = page.getFreeSpace();
    std::uint16_t slot = 0;

    CHECK(Serializer::serialize(node, {}, page, slot));
    CHECK(page.getNumRecords() == 1);
    CHECK(page.getFreeSpace() < freeBefore);
}

// childPageIds com tamanho diferente de childCount() é erro de uso.
void test_serialize_throwsWhenChildIdsMismatch() {
    BTreeNode node(3, true); // 0 filhos
    Page page;
    std::uint16_t slot = 0;

    bool threw = false;
    try {
        Serializer::serialize(node, {1, 2}, page, slot);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    CHECK(threw);
    CHECK(page.getNumRecords() == 0);
}

// Page sem espaço: serialize devolve false (sem lançar exceção).
void test_serialize_returnsFalseWhenPageIsFull() {
    Page page;
    std::uint16_t slot = 0;

    // Enche a Page com registros dummy até não caber mais nada.
    const Bytes filler(64, 0xAB);
    while (insertRaw(page, filler, slot)) {
    }
    const std::uint16_t recordsBefore = page.getNumRecords();

    BTreeNode node(3, true);
    node.insertKey(1);
    CHECK(!Serializer::serialize(node, {}, page, slot));
    CHECK(page.getNumRecords() == recordsBefore);
}

// Nó que nunca caberia em uma Page: false e a Page fica intacta.
void test_serialize_returnsFalseWhenNodeExceedsPageSize() {
    BTreeNode node(600, true); // até 1199 chaves => 1199 * 4 bytes > 4096
    for (int i = 0; static_cast<std::size_t>(i) < node.maxKeys(); ++i) {
        node.insertKey(i);
    }

    Page page;
    const std::uint16_t freeBefore = page.getFreeSpace();
    std::uint16_t slot = 0;

    CHECK(!Serializer::serialize(node, {}, page, slot));
    CHECK(page.getNumRecords() == 0);
    CHECK(page.getFreeSpace() == freeBefore);
}

// ---------------------------------------------------------------------------
// deserialize()
// ---------------------------------------------------------------------------

// serialize -> deserialize preserva tipo, grau mínimo e chaves (incl. negativas).
void test_roundTrip_leafWithKeys() {
    BTreeNode node(3, true);
    node.insertKey(-100);
    node.insertKey(0);
    node.insertKey(42);

    Page page;
    std::uint16_t slot = 0;
    CHECK(Serializer::serialize(node, {}, page, slot));

    Ids childIds{99}; // deve ser limpo pelo deserialize
    auto result = Serializer::deserialize(page, slot, childIds);

    CHECK(result.has_value());
    if (!result) return;
    CHECK(result->isLeaf());
    CHECK(result->minDegree() == 3);
    CHECK(sameKeys(*result, node));
    CHECK(childIds.empty());
    CHECK(result->validate());
}

// Nó folha com capacidade máxima (2t - 1 chaves).
void test_roundTrip_fullLeaf() {
    BTreeNode node(2, true);
    for (int k : {30, 10, 20}) node.insertKey(k);
    CHECK(node.isFull());

    Page page;
    std::uint16_t slot = 0;
    CHECK(Serializer::serialize(node, {}, page, slot));

    Ids childIds;
    auto result = Serializer::deserialize(page, slot, childIds);

    CHECK(result.has_value());
    if (!result) return;
    CHECK(result->isFull());
    CHECK(sameKeys(*result, node));
}

// Nó interno vazio (flag leaf = false) sobrevive ao round trip.
void test_roundTrip_emptyInternalNode() {
    BTreeNode node(2, false);

    Page page;
    std::uint16_t slot = 0;
    CHECK(Serializer::serialize(node, {}, page, slot));

    Ids childIds;
    auto result = Serializer::deserialize(page, slot, childIds);

    CHECK(result.has_value());
    if (!result) return;
    CHECK(!result->isLeaf());
    CHECK(result->minDegree() == 2);
    CHECK(result->isEmpty());
}

// Fluxo completo da BTree: nó interno + filhos, cada nó em sua própria Page.
// Depois de ler a Page do pai, os filhos são carregados pelos pageIds e
// religados com insertChild().
void test_roundTrip_internalNodeWithChildren() {
    // Monta pai (2 chaves) e 3 filhos folha.
    BTreeNode c0(2, true), c1(2, true), c2(2, true);
    c0.insertKey(1);
    c1.insertKey(15);
    c2.insertKey(30);

    BTreeNode parent(2, false);
    parent.insertKey(10);
    parent.insertKey(20);
    parent.insertChild(0, &c0);
    parent.insertChild(1, &c1);
    parent.insertChild(2, &c2);
    CHECK(parent.validate());

    // "Disco": pageId -> Page.
    const Ids childIds{11, 22, 33};
    const BTreeNode* children[] = {&c0, &c1, &c2};
    std::map<std::uint32_t, Page> disk;
    std::map<std::uint32_t, std::uint16_t> slotOf;

    for (std::size_t i = 0; i < childIds.size(); ++i) {
        Page& p = disk[childIds[i]];
        p.init(childIds[i]);
        CHECK(Serializer::serialize(*children[i], {}, p, slotOf[childIds[i]]));
    }
    Page& parentPage = disk[1];
    parentPage.init(1);
    std::uint16_t parentSlot = 0;
    CHECK(Serializer::serialize(parent, childIds, parentPage, parentSlot));

    // Lê o pai: vem sem filhos, mas com os pageIds.
    Ids loadedIds;
    auto restored = Serializer::deserialize(disk[1], parentSlot, loadedIds);
    CHECK(restored.has_value());
    if (!restored) return;
    CHECK(!restored->isLeaf());
    CHECK(sameKeys(*restored, parent));
    CHECK(restored->childCount() == 0);
    CHECK(loadedIds == childIds);

    // Carrega e religa cada filho.
    std::vector<std::unique_ptr<BTreeNode>> owned;
    for (std::size_t i = 0; i < loadedIds.size(); ++i) {
        auto child = Serializer::deserialize(disk[loadedIds[i]],
                                             slotOf[loadedIds[i]], loadedIds);
        CHECK(child.has_value());
        if (!child) return;
        owned.push_back(std::make_unique<BTreeNode>(std::move(*child)));
        restored->insertChild(i, owned.back().get());
        loadedIds = childIds; // deserialize de folha limpa o vetor
    }

    CHECK(restored->childCount() == 3);
    CHECK(restored->validate());
    for (std::size_t i = 0; i < 3; ++i) {
        CHECK(sameKeys(*restored->getChild(i), *children[i]));
    }
}

// Dois nós na mesma Page ocupam slots distintos e não se misturam.
void test_roundTrip_twoNodesInSamePage() {
    BTreeNode a(2, true), b(3, true);
    a.insertKey(1);
    b.insertKey(7);
    b.insertKey(8);

    Page page;
    std::uint16_t slotA = 0, slotB = 0;
    CHECK(Serializer::serialize(a, {}, page, slotA));
    CHECK(Serializer::serialize(b, {}, page, slotB));
    CHECK(slotA != slotB);

    Ids ids;
    auto ra = Serializer::deserialize(page, slotA, ids);
    auto rb = Serializer::deserialize(page, slotB, ids);
    CHECK(ra.has_value() && rb.has_value());
    if (!ra || !rb) return;
    CHECK(ra->minDegree() == 2 && sameKeys(*ra, a));
    CHECK(rb->minDegree() == 3 && sameKeys(*rb, b));
}

// Os pageIds dos filhos são devolvidos na ordem gravada (registro montado à
// mão, cobrindo também valores extremos de uint32).
void test_deserialize_returnsChildPageIdsInOrder() {
    const Ids expected{7, 0xFFFFFFFFu, 3};
    Page page;
    std::uint16_t slot = 0;
    CHECK(insertRaw(page, makeRecord(0, 2, {10, 20}, expected), slot));

    Ids childIds;
    auto result = Serializer::deserialize(page, slot, childIds);

    CHECK(result.has_value());
    CHECK(childIds == expected);
}

// Slot que nunca existiu e slot de registro removido.
void test_deserialize_invalidOrDeletedSlotReturnsNullopt() {
    Page page;
    Ids childIds{1};

    CHECK(!Serializer::deserialize(page, 123, childIds).has_value());
    CHECK(childIds.empty());

    BTreeNode node(2, true);
    node.insertKey(5);
    std::uint16_t slot = 0;
    CHECK(Serializer::serialize(node, {}, page, slot));
    CHECK(page.deleteRecord(slot));
    CHECK(!Serializer::deserialize(page, slot, childIds).has_value());
}

// Registros corrompidos ou rejeitados pelo BTreeNode resultam em nullopt.
void test_deserialize_corruptedRecordsReturnNullopt() {
    Ids childIds;
    std::uint16_t slot = 0;

    { // byte de folha inválido (só 0 ou 1 são aceitos)
        Page page;
        insertRaw(page, makeRecord(2, 3, {1}, {}), slot);
        CHECK(!Serializer::deserialize(page, slot, childIds).has_value());
    }
    { // registro truncado (falta o último byte)
        Page page;
        Bytes rec = makeRecord(1, 3, {1, 2}, {});
        rec.pop_back();
        insertRaw(page, rec, slot);
        CHECK(!Serializer::deserialize(page, slot, childIds).has_value());
    }
    { // bytes sobrando no final
        Page page;
        Bytes rec = makeRecord(1, 3, {1, 2}, {});
        rec.push_back(0x00);
        insertRaw(page, rec, slot);
        CHECK(!Serializer::deserialize(page, slot, childIds).has_value());
    }
    { // grau mínimo inválido (BTreeNode exige >= 2)
        Page page;
        insertRaw(page, makeRecord(1, 1, {1}, {}), slot);
        CHECK(!Serializer::deserialize(page, slot, childIds).has_value());
    }
    { // chaves duplicadas
        Page page;
        insertRaw(page, makeRecord(1, 3, {5, 5}, {}), slot);
        CHECK(!Serializer::deserialize(page, slot, childIds).has_value());
    }
    { // excesso de chaves: t = 2 aceita no máximo 3
        Page page;
        insertRaw(page, makeRecord(1, 2, {1, 2, 3, 4}, {}), slot);
        CHECK(!Serializer::deserialize(page, slot, childIds).has_value());
    }
}

} // namespace

// ---------------------------------------------------------------------------

int main() {
    RUN(test_serialize_writesExpectedFormat);
    RUN(test_serialize_storesOneRecordInPage);
    RUN(test_serialize_throwsWhenChildIdsMismatch);
    RUN(test_serialize_returnsFalseWhenPageIsFull);
    RUN(test_serialize_returnsFalseWhenNodeExceedsPageSize);
    RUN(test_roundTrip_leafWithKeys);
    RUN(test_roundTrip_fullLeaf);
    RUN(test_roundTrip_emptyInternalNode);
    RUN(test_roundTrip_internalNodeWithChildren);
    RUN(test_roundTrip_twoNodesInSamePage);
    RUN(test_deserialize_returnsChildPageIdsInOrder);
    RUN(test_deserialize_invalidOrDeletedSlotReturnsNullopt);
    RUN(test_deserialize_corruptedRecordsReturnNullopt);

    if (g_failures == 0) {
        std::cout << "\nTodos os testes passaram.\n";
        return 0;
    }
    std::cerr << "\n" << g_failures << " verificação(ões) falharam.\n";
    return 1;
}
