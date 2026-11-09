#include "Page.hpp"

#include <cstring>

namespace {
// Valor retornado por findFreeSlot() quando não há slot livre reutilizável.
constexpr std::uint16_t NO_FREE_SLOT = 0xFFFF;
}

Page::Page() {
    init(0);
}

void Page::init(std::uint32_t pageId) {
    std::memset(&header_, 0, sizeof(header_));
    header_.pageId = pageId;
    header_.numRecords = 0;
    header_.numSlots = 0;
    header_.freeSpaceOffset = 0;
    header_.freeSpace = PAGE_SIZE;
}

// ---------------------------------------------------------------------------
// Operações públicas
// ---------------------------------------------------------------------------

bool Page::insertRecord(const void* data, std::uint16_t size, std::uint16_t& slotId) {
    if (data == nullptr || size == 0) {
        return false;
    }

    // Reutiliza um slot livre, se houver; caso contrário, aloca um novo.
    std::uint16_t targetSlot = findFreeSlot();
    const bool reuseSlot = (targetSlot != NO_FREE_SLOT);
    if (!reuseSlot) {
        targetSlot = header_.numSlots;
    }

    const std::size_t required = static_cast<std::size_t>(size) + (reuseSlot ? 0 : SLOT_SIZE);
    if (required > header_.freeSpace) {
        return false;
    }

    std::memcpy(header_.pageData + header_.freeSpaceOffset, data, size);

    if (!reuseSlot) {
        ++header_.numSlots;
    }
    writeSlot(targetSlot, Slot{header_.freeSpaceOffset, size});

    header_.freeSpaceOffset = static_cast<std::uint16_t>(header_.freeSpaceOffset + size);
    header_.freeSpace = static_cast<std::uint16_t>(header_.freeSpace - required);
    ++header_.numRecords;

    slotId = targetSlot;
    return true;
}

bool Page::getRecord(std::uint16_t slotId, void* outBuffer,
                     std::uint16_t bufferSize, std::uint16_t& outSize) const {
    if (!isActiveSlot(slotId)) {
        return false;
    }

    const Slot slot = readSlot(slotId);
    outSize = slot.length;

    if (outBuffer == nullptr || bufferSize < slot.length) {
        return false;
    }

    std::memcpy(outBuffer, header_.pageData + slot.offset, slot.length);
    return true;
}

bool Page::updateRecord(std::uint16_t slotId, const void* data, std::uint16_t size) {
    if (data == nullptr || size == 0 || !isActiveSlot(slotId)) {
        return false;
    }

    const Slot oldSlot = readSlot(slotId);

    // Só o crescimento líquido precisa caber no espaço livre; o slot é reaproveitado.
    if (size > oldSlot.length && (size - oldSlot.length) > header_.freeSpace) {
        return false;
    }

    // Remove o registro antigo (compactando) e reescreve ao final da área de dados.
    // O slot é mantido para que o slotId continue válido.
    removeRecordBytes(slotId);

    std::memcpy(header_.pageData + header_.freeSpaceOffset, data, size);
    writeSlot(slotId, Slot{header_.freeSpaceOffset, size});

    header_.freeSpaceOffset = static_cast<std::uint16_t>(header_.freeSpaceOffset + size);
    header_.freeSpace = static_cast<std::uint16_t>(header_.freeSpace - size);
    return true;
}

bool Page::deleteRecord(std::uint16_t slotId) {
    if (!isActiveSlot(slotId)) {
        return false;
    }

    removeRecordBytes(slotId);
    --header_.numRecords;
    trimTrailingFreeSlots();
    return true;
}

std::uint16_t Page::getFreeSpace() const {
    return header_.freeSpace;
}

std::uint32_t Page::getPageId() const {
    return header_.pageId;
}

std::uint16_t Page::getNumRecords() const {
    return header_.numRecords;
}

// ---------------------------------------------------------------------------
// Auxiliares privados
// ---------------------------------------------------------------------------

std::size_t Page::slotPosition(std::uint16_t slotId) {
    return PAGE_SIZE - (static_cast<std::size_t>(slotId) + 1) * SLOT_SIZE;
}

Page::Slot Page::readSlot(std::uint16_t slotId) const {
    Slot slot;
    // memcpy evita problemas de alinhamento ao acessar o buffer de bytes.
    std::memcpy(&slot, header_.pageData + slotPosition(slotId), SLOT_SIZE);
    return slot;
}

void Page::writeSlot(std::uint16_t slotId, const Slot& slot) {
    std::memcpy(header_.pageData + slotPosition(slotId), &slot, SLOT_SIZE);
}

bool Page::isActiveSlot(std::uint16_t slotId) const {
    return slotId < header_.numSlots && readSlot(slotId).length != 0;
}

std::uint16_t Page::findFreeSlot() const {
    for (std::uint16_t i = 0; i < header_.numSlots; ++i) {
        if (readSlot(i).length == 0) {
            return i;
        }
    }
    return NO_FREE_SLOT;
}

void Page::removeRecordBytes(std::uint16_t slotId) {
    const Slot removed = readSlot(slotId);
    const std::uint16_t tailStart = static_cast<std::uint16_t>(removed.offset + removed.length);
    const std::uint16_t tailSize = static_cast<std::uint16_t>(header_.freeSpaceOffset - tailStart);

    // Desloca os registros seguintes para preencher o buraco.
    if (tailSize > 0) {
        std::memmove(header_.pageData + removed.offset,
                     header_.pageData + tailStart, tailSize);
    }

    // Marca o slot como livre antes de ajustar os demais, para que ele seja ignorado.
    writeSlot(slotId, Slot{0, 0});

    for (std::uint16_t i = 0; i < header_.numSlots; ++i) {
        Slot slot = readSlot(i);
        if (slot.length != 0 && slot.offset > removed.offset) {
            slot.offset = static_cast<std::uint16_t>(slot.offset - removed.length);
            writeSlot(i, slot);
        }
    }

    header_.freeSpaceOffset = static_cast<std::uint16_t>(header_.freeSpaceOffset - removed.length);
    header_.freeSpace = static_cast<std::uint16_t>(header_.freeSpace + removed.length);
}

void Page::trimTrailingFreeSlots() {
    while (header_.numSlots > 0 && readSlot(static_cast<std::uint16_t>(header_.numSlots - 1)).length == 0) {
        --header_.numSlots;
        header_.freeSpace = static_cast<std::uint16_t>(header_.freeSpace + SLOT_SIZE);
    }
}
