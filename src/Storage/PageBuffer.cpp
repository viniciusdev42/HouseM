#include "PageBuffer.hpp"

#include <iterator>
#include <stdexcept>

PageBuffer::PageBuffer(std::size_t capacity, IPageStorage& storage)
    : storage_(storage), frames_(capacity) {
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
    if (!storage_.readPage(pageId, frame.page)) {
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
        // Mais recentemente usada fica no fim; a menos recente, na frente.
        lruList_.push_back(frameIndex);
        frame.lruPosition = std::prev(lruList_.end());
    }
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
        return NO_FRAME; // falha ao gravar a Page suja: ela permanece no buffer
    }
    return victim;
}

bool PageBuffer::evictFrame(std::size_t frameIndex) {
    Frame& frame = frames_[frameIndex];

    if (frame.dirty && !storage_.writePage(frame.pageId, frame.page)) {
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
