#ifndef PAGE_BUFFER_HPP
#define PAGE_BUFFER_HPP

#include <cstddef>
#include <cstdint>
#include <list>
#include <unordered_map>
#include <vector>

#include "src/Main/Model/Storage/Page/Page.hpp"
#include "src/Main/Model/Storage/DiskManager/DiskManager.hpp"

/**
 * Buffer pool de Pages em memória, com política de substituição LRU.
 *
 * Uma Page "pinada" (pinCount > 0) está em uso e nunca é substituída.
 * Quando o pinCount chega a zero, a Page torna-se candidata à substituição.
 * Páginas sujas são gravadas no disco antes de serem substituídas.
 *
 * Esta classe não é thread-safe.
 */
class PageBuffer {
public:
    /**
     * @param capacity Número de frames (Pages) mantidos em memória (> 0).
     * @param diskManager Gerenciador de leitura/gravação em disco. Deve
     *                    sobreviver ao PageBuffer.
     * @throws std::invalid_argument se capacity == 0.
     */
    PageBuffer(std::size_t capacity, DiskManager& diskManager);

    PageBuffer(const PageBuffer&) = delete;
    PageBuffer& operator=(const PageBuffer&) = delete;

    /**
     * Obtém a Page e a pina (incrementa o pinCount).
     * Se a Page não estiver no buffer, ela é lida do disco,
     * substituindo (via LRU) uma Page não pinada se necessário.
     *
     * @return Ponteiro para a Page, válido enquanto ela permanecer pinada;
     *         nullptr se todos os frames estiverem pinados, se a leitura
     *         falhar ou se a gravação de uma Page suja a ser substituída falhar.
     */
    Page* fetch(std::uint32_t pageId);

    /**
     * Incrementa o pinCount de uma Page que já está no buffer.
     * @return true em caso de sucesso; false se a Page não estiver no buffer.
     */
    bool pin(std::uint32_t pageId);

    /**
     * Decrementa o pinCount da Page.
     * @param isDirty true se o chamador modificou a Page; marca o frame como sujo
     *                (a marca nunca é removida por um unpin com false).
     * @return true em caso de sucesso; false se a Page não estiver no buffer
     *         ou se seu pinCount já for zero.
     */
    bool unpin(std::uint32_t pageId, bool isDirty);
        /**
     * Cria no buffer uma Page nova e vazia com o pageId informado, sem
     * consultar o disco. A Page é inicializada, marcada como suja (para que o
     * primeiro descarte a grave) e retornada pinada (pinCount = 1).
     *
     * @return Ponteiro para a Page; nullptr se o pageId já estiver no buffer,
     *         se todos os frames estiverem pinados, se a gravação de uma Page
     *         suja a ser substituída falhar ou se faltar memória.
     */
    Page* newPage(std::uint32_t pageId);

    /**
     * Remove a Page do buffer SEM gravá-la no disco (usado quando a Page foi
     * liberada e seu conteúdo não importa mais).
     *
     * @return true se a Page foi descartada ou não estava no buffer;
     *         false se ela estiver pinada (continua em uso).
     */
    bool discard(std::uint32_t pageId);

private:
    struct Frame {
        Page          page;
        std::uint32_t pageId   = 0;
        std::uint32_t pinCount = 0;
        bool          dirty    = false;
        bool          inUse    = false;                 // frame contém uma Page válida
        std::list<std::size_t>::iterator lruPosition;   // válido só se pinCount == 0 e inUse
    };

    static constexpr std::size_t NO_FRAME = static_cast<std::size_t>(-1);

    /** Retorna um frame livre, substituindo uma Page se preciso; NO_FRAME se impossível. */
    std::size_t acquireFrame();

    /** Grava (se suja) e remove a Page do frame, deixando-o livre. */
    bool evictFrame(std::size_t frameIndex);

    /** Incrementa o pinCount do frame, removendo-o da lista LRU se necessário. */
    void pinFrame(std::size_t frameIndex);

    DiskManager&                                  diskManager_;
    std::vector<Frame>                            frames_;
    std::unordered_map<std::uint32_t, std::size_t> pageTable_; // pageId -> índice do frame
    std::vector<std::size_t>                      freeFrames_;
    std::list<std::size_t>                        lruList_;   // não pinados; frente = menos recente
};

#endif // PAGE_BUFFER_HPP