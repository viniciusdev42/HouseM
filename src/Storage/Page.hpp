#ifndef PAGE
#define PAGE

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <vector>

namespace Page {

using PageId = std::uint32_t;
using SlotId = std::uint16_t;

constexpr std::size_t PAGE_SIZE = 4096;

// Estado de cada entrada do diretório de slots.
//  - Empty  : slot livre, pode ser reutilizado por um novo insertRecord.
//  - Active : registro válido e legível.
//  - Removed: remoção lógica (removeRecord). Os bytes continuam na página,
//             mas o registro não é mais legível nem atualizável.
enum class SlotState : std::uint8_t { Empty = 0, Active = 1, Removed = 2 };

struct Slot {
    std::uint16_t offset{0};  // posição do registro dentro do payload
    std::uint16_t length{0};  // tamanho do registro em bytes
    SlotState state{SlotState::Empty};
};

// Campos de controle ocupam 8 bytes: pageId (4) + slotCount (2) + freeSpaceOffset (2).
constexpr std::size_t HEADER_CONTROL_SIZE =
    sizeof(PageId) + 2 * sizeof(std::uint16_t);
constexpr std::size_t PAYLOAD_SIZE = PAGE_SIZE - HEADER_CONTROL_SIZE;
constexpr std::size_t MAX_RECORD_SIZE = PAYLOAD_SIZE - sizeof(Slot);

static_assert(PAYLOAD_SIZE <= UINT16_MAX,
              "offsets e tamanhos precisam caber em uint16_t");

// Cabeçalho da página: identificador, metadados de controle e o payload.
struct PageHeader {
    PageId pageId{0};
    std::uint16_t slotCount{0};         // quantidade de entradas no diretório de slots
    std::uint16_t freeSpaceOffset{0};   // início da área de registros (cresce para baixo)
    std::array<std::uint8_t, PAYLOAD_SIZE> payload{};
};

static_assert(sizeof(PageHeader) == PAGE_SIZE, "a página deve ocupar exatamente PAGE_SIZE bytes");

// Layout do payload (slotted page):
//
//  | slot 0 | slot 1 | ... | espaço livre | ... | registro 1 | registro 0 |
//  ^ início do payload                                       fim do payload ^
//
// O diretório de slots cresce do início para o fim; os registros crescem do
// fim para o início. O slotId é estável durante toda a vida do registro,
// mesmo quando a página é compactada.
class Page {
public:
    explicit Page(PageId id) {
        header_.pageId = id;
        header_.slotCount = 0;
        header_.freeSpaceOffset = static_cast<std::uint16_t>(PAYLOAD_SIZE);
    }

    PageId pageId() const { return header_.pageId; }

    // Insere um registro. Retorna o slotId, ou nullopt se o registro for
    // vazio, grande demais ou não houver espaço.
    std::optional<SlotId> insertRecord(std::span<const std::uint8_t> record) {
        if (record.empty() || record.size() > MAX_RECORD_SIZE) return std::nullopt;

        const std::optional<SlotId> reusable = findEmptySlot();
        const std::size_t slotCost = reusable ? 0 : sizeof(Slot);
        if (!ensureContiguousSpace(record.size() + slotCost)) return std::nullopt;

        SlotId id;
        if (reusable) {
            id = *reusable;
        } else {
            id = header_.slotCount++;
        }

        const auto offset = static_cast<std::uint16_t>(header_.freeSpaceOffset - record.size());
        std::memcpy(header_.payload.data() + offset, record.data(), record.size());
        header_.freeSpaceOffset = offset;

        writeSlot(id, Slot{offset, static_cast<std::uint16_t>(record.size()), SlotState::Active});
        return id;
    }

    // Lê (copia) um registro ativo. Retorna nullopt se o slot for inválido,
    // estiver vazio ou tiver sido removido logicamente.
    std::optional<std::vector<std::uint8_t>> readRecord(SlotId id) const {
        const std::optional<Slot> slot = activeSlot(id);
        if (!slot) return std::nullopt;

        const auto begin = header_.payload.begin() + slot->offset;
        return std::vector<std::uint8_t>(begin, begin + slot->length);
    }

    // Substitui o conteúdo de um registro ativo, mantendo o mesmo slotId.
    // Retorna false (sem alterar nada) se o slot for inválido ou se o novo
    // conteúdo não couber na página.
    // Observação: o span não deve apontar para dentro do payload desta página.
    bool updateRecord(SlotId id, std::span<const std::uint8_t> record) {
        std::optional<Slot> slot = activeSlot(id);
        if (!slot || record.empty() || record.size() > MAX_RECORD_SIZE) return false;

        // Cabe no espaço atual: atualiza no lugar (a sobra vira espaço
        // recuperável na próxima compactação).
        if (record.size() <= slot->length) {
            std::memcpy(header_.payload.data() + slot->offset, record.data(), record.size());
            slot->length = static_cast<std::uint16_t>(record.size());
            writeSlot(id, *slot);
            return true;
        }

        // Cresceu: verifica se cabe antes de tocar na página, para não perder dados.
        const std::size_t directoryEnd = header_.slotCount * sizeof(Slot);
        const std::size_t othersBytes = recordBytesInUse(id);
        if (directoryEnd + othersBytes + record.size() > PAYLOAD_SIZE) return false;

        // Libera os bytes antigos, compacta e grava o novo conteúdo.
        slot->length = 0;
        writeSlot(id, *slot);
        compact();

        slot->offset = static_cast<std::uint16_t>(header_.freeSpaceOffset - record.size());
        slot->length = static_cast<std::uint16_t>(record.size());
        std::memcpy(header_.payload.data() + slot->offset, record.data(), record.size());
        header_.freeSpaceOffset = slot->offset;
        writeSlot(id, *slot);
        return true;
    }

    // Remoção LÓGICA: marca o registro como removido. O espaço não é
    // devolvido e o slot continua reservado (o registro pode ser
    // "recuperado" por ferramentas de baixo nível ou apagado de vez
    // com deleteRecord).
    bool removeRecord(SlotId id) {
        std::optional<Slot> slot = activeSlot(id);
        if (!slot) return false;

        slot->state = SlotState::Removed;
        writeSlot(id, *slot);
        return true;
    }

    // Remoção FÍSICA: libera o slot e os bytes do registro (ativo ou
    // removido logicamente), compacta a página e encolhe o diretório
    // quando os últimos slots ficam vazios.
    bool deleteRecord(SlotId id) {
        if (id >= header_.slotCount) return false;

        const Slot slot = readSlot(id);
        if (slot.state == SlotState::Empty) return false;

        writeSlot(id, Slot{});
        trimTrailingEmptySlots();
        compact();
        return true;
    }

private:
    PageHeader header_;

    // ---------- acesso ao diretório de slots ----------

    Slot readSlot(SlotId id) const {
        Slot slot;
        std::memcpy(&slot, header_.payload.data() + id * sizeof(Slot), sizeof(Slot));
        return slot;
    }

    void writeSlot(SlotId id, const Slot& slot) {
        std::memcpy(header_.payload.data() + id * sizeof(Slot), &slot, sizeof(Slot));
    }

    std::optional<Slot> activeSlot(SlotId id) const {
        if (id >= header_.slotCount) return std::nullopt;
        const Slot slot = readSlot(id);
        if (slot.state != SlotState::Active) return std::nullopt;
        return slot;
    }

    std::optional<SlotId> findEmptySlot() const {
        for (SlotId id = 0; id < header_.slotCount; ++id) {
            if (readSlot(id).state == SlotState::Empty) return id;
        }
        return std::nullopt;
    }

    void trimTrailingEmptySlots() {
        while (header_.slotCount > 0 &&
               readSlot(static_cast<SlotId>(header_.slotCount - 1)).state == SlotState::Empty) {
            --header_.slotCount;
        }
    }

    // ---------- gerenciamento de espaço ----------

    std::size_t contiguousFreeSpace() const {
        return header_.freeSpaceOffset - header_.slotCount * sizeof(Slot);
    }

    // Soma dos bytes de registros que precisam continuar na página,
    // ignorando opcionalmente um slot.
    std::size_t recordBytesInUse(std::optional<SlotId> ignore = std::nullopt) const {
        std::size_t total = 0;
        for (SlotId id = 0; id < header_.slotCount; ++id) {
            if (ignore && *ignore == id) continue;
            const Slot slot = readSlot(id);
            if (slot.state != SlotState::Empty) total += slot.length;
        }
        return total;
    }

    bool ensureContiguousSpace(std::size_t needed) {
        if (contiguousFreeSpace() >= needed) return true;
        compact();
        return contiguousFreeSpace() >= needed;
    }

    // Reagrupa os registros no fim do payload, eliminando buracos.
    // Os slotIds não mudam; apenas os offsets são atualizados.
    void compact() {
        std::vector<SlotId> ids;
        for (SlotId id = 0; id < header_.slotCount; ++id) {
            const Slot slot = readSlot(id);
            if (slot.state != SlotState::Empty && slot.length > 0) ids.push_back(id);
        }

        // Do registro mais ao fim para o mais ao início: cada registro só
        // se move para a direita, então nunca sobrescreve um ainda não movido.
        std::sort(ids.begin(), ids.end(), [this](SlotId a, SlotId b) {
            return readSlot(a).offset > readSlot(b).offset;
        });

        std::size_t cursor = PAYLOAD_SIZE;
        for (const SlotId id : ids) {
            Slot slot = readSlot(id);
            cursor -= slot.length;
            std::memmove(header_.payload.data() + cursor,
                         header_.payload.data() + slot.offset, slot.length);
            slot.offset = static_cast<std::uint16_t>(cursor);
            writeSlot(id, slot);
        }
        header_.freeSpaceOffset = static_cast<std::uint16_t>(cursor);
    }
};

}  // namespace Page

#endif
