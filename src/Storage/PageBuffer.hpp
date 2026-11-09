#ifndef PAGE_BUFFER
#define PAGE_BUFFER

#include <cstddef>
#include <cstdint>
#include <list>
#include <unordered_map>
#include <vector>

#include "Page.hpp"

/**
 * Abstração do armazenamento persistente das Pages (disco, arquivo, etc.).
 *
 * O PageBuffer depende apenas desta interface, o que o mantém desacoplado
 * da forma como as Pages são lidas e gravadas.
 */
class IPageStorage {
public:
    virtual ~IPageStorage() = default;

    /** Lê a Page identificada por pageId para dentro de `page`. */
    virtual bool readPage(std::uint32_t pageId, Page& page) = 0;

    /** Grava `page` no armazenamento sob o identificador pageId. */
    virtual bool writePage(std::uint32_t pageId, const Page& page) = 0;
};

/**
 * Buffer pool de Pages em memória, com política de substituição LRU.
 *
 * Uma Page "pinada" (pinCount > 0) está em uso e nunca é substituída.
 * Quando o pinCount chega a zero, a Page torna-se candidata à substituição.
 * Páginas sujas são gravadas no armazenamento antes de serem substituídas.
 *
 * Esta classe não é thread-safe.
 */
class PageBuffer {
public:
    /**
     * @param capacity Número de frames (Pages) mantidos em memória (> 0).
     * @param storage  Armazenamento usado para ler/gravar Pages. Deve
     *                 sobreviver ao PageBuffer.
     * @throws std::invalid_argument se capacity == 0.
     */
    PageBuffer(std::size_t capacity, IPageStorage& storage);

    PageBuffer(const PageBuffer&) = delete;
    PageBuffer& operator=(const PageBuffer&) = delete;

    /**
     * Obtém a Page e a pina (incrementa o pinCount).
     * Se a Page não estiver no buffer, ela é lida do armazenamento,
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

    /** Retorna o índice de um frame livre, substituindo uma Page se preciso; NO_FRAME se impossível. */
    std::size_t acquireFrame();

    /** Grava (se suja) e remove a Page do frame, deixando-o livre. */
    bool evictFrame(std::size_t frameIndex);

    /** Incrementa o pinCount do frame, removendo-o da lista LRU se necessário. */
    void pinFrame(std::size_t frameIndex);

    IPageStorage&                                  storage_;
    std::vector<Frame>                             frames_;
    std::unordered_map<std::uint32_t, std::size_t> pageTable_; // pageId -> índice do frame
    std::vector<std::size_t>                       freeFrames_;
    std::list<std::size_t>                         lruList_;   // frames não pinados; frente = menos recente
};

#endif // PAGE_BUFFER
