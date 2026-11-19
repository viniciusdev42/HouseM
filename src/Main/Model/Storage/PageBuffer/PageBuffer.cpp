#include "PageBuffer.hpp"

#include <iterator>
#include <stdexcept>
#include <new> /*std::bad_alloc*/

PageBuffer::PageBuffer(std::size_t capacity, DiskManager& diskManager)
    : diskManager_(diskManager), frames_(capacity) {
    if (capacity == 0) {
        throw std::invalid_argument("PageBuffer: a capacidade deve ser maior que zero");
    }

    freeFrames_.reserve(capacity);
    for (std::size_t i = capacity; i > 0; --i) {
        freeFrames_.push_back(i - 1);
    }
    pageTable_.reserve(capacity);
}

Page* PageBuffer::fetch(std::uint32_t pageId) {
    const auto found = pageTable_.find(pageId);
    if (found != pageTable_.end()) {
        pinFrame(found->second);
        return &frames_[found->second].page;
    }

    const std::size_t frameIndex = acquireFrame();
    if (frameIndex == NO_FRAME) {
        return nullptr;
    }

    Frame& frame = frames_[frameIndex];
    if (!diskManager_.readPage(pageId, frame.page)) {
        freeFrames_.push_back(frameIndex);
        return nullptr;
    }

    frame.pageId   = pageId;
    frame.pinCount = 1;
    frame.dirty    = false;
    frame.inUse    = true;
    pageTable_[pageId] = frameIndex;

    return &frame.page;
}

bool PageBuffer::pin(std::uint32_t pageId) {
    const auto found = pageTable_.find(pageId);
    if (found == pageTable_.end()) {
        return false;
    }

    pinFrame(found->second);
    return true;
}

bool PageBuffer::unpin(std::uint32_t pageId, bool isDirty) {
    const auto found = pageTable_.find(pageId);
    if (found == pageTable_.end()) {
        return false;
    }

    const std::size_t frameIndex = found->second;
    Frame& frame = frames_[frameIndex];
    if (frame.pinCount == 0) {
        return false;
    }

    if (isDirty) {
        frame.dirty = true;
    }

    --frame.pinCount;
    if (frame.pinCount == 0) {
        // A Page mais recentemente usada fica no fim da lista LRU.
        lruList_.push_back(frameIndex);
        frame.lruPosition = std::prev(lruList_.end());
    }
    return true;
}

Page* PageBuffer::newPage(std::uint32_t pageId) {
    // Um id ainda presente no buffer está em uso; não pode ser recriado.
    if (pageTable_.find(pageId) != pageTable_.end()) {
        return nullptr;
    }

    const std::size_t frameIndex = acquireFrame();
    if (frameIndex == NO_FRAME) {
        return nullptr;
    }

    try {
        pageTable_.emplace(pageId, frameIndex);
    } catch (const std::bad_alloc&) {
        freeFrames_.push_back(frameIndex); // capacidade já reservada no construtor
        return nullptr;
    }

    Frame& frame = frames_[frameIndex];
    frame.page.init(pageId);
    frame.pageId   = pageId;
    frame.pinCount = 1;
    frame.dirty    = true;   // não existe no disco ainda
    frame.inUse    = true;

    return &frame.page;
}

bool PageBuffer::discard(std::uint32_t pageId) {
    const auto found = pageTable_.find(pageId);
    if (found == pageTable_.end()) {
        return true; // nada a descartar
    }

    const std::size_t frameIndex = found->second;
    Frame& frame = frames_[frameIndex];
    if (frame.pinCount > 0) {
        return false; // em uso: não pode ser descartada
    }

    lruList_.erase(frame.lruPosition);
    pageTable_.erase(found);
    frame.inUse = false;
    frame.dirty = false;    // descarta sem gravar
    freeFrames_.push_back(frameIndex);
    return true;
}

std::size_t PageBuffer::acquireFrame() {
    if (!freeFrames_.empty()) {
        const std::size_t frameIndex = freeFrames_.back();
        freeFrames_.pop_back();
        return frameIndex;
    }

    if (lruList_.empty()) {
        return NO_FRAME; // todos os frames estão pinados
    }

    const std::size_t victim = lruList_.front();
    if (!evictFrame(victim)) {
        return NO_FRAME; // a Page suja permanece no buffer se a gravação falhar
    }
    return victim;
}

bool PageBuffer::evictFrame(std::size_t frameIndex) {
    Frame& frame = frames_[frameIndex];

    // DiskManager obtém o pageId diretamente da Page.
    if (frame.dirty && !diskManager_.writePage(frame.page)) {
        return false;
    }

    lruList_.erase(frame.lruPosition);
    pageTable_.erase(frame.pageId);
    frame.inUse = false;
    frame.dirty = false;
    frame.pinCount = 0;
    return true;
}

void PageBuffer::pinFrame(std::size_t frameIndex) {
    Frame& frame = frames_[frameIndex];
    if (frame.pinCount == 0) {
        lruList_.erase(frame.lruPosition);
    }
    ++frame.pinCount;
}