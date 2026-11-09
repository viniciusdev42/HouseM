#ifndef PAGE
#define PAGE

#include <cstddef>
#include <cstdint>

/**
 * Tamanho da área de dados de uma Page: 4KB.
 */
constexpr std::uint16_t PAGE_SIZE = 4096;

/**
 * Cabeçalho da Page.
 *
 * Campos obrigatórios: pageId, pageData, numRecords e freeSpace.
 * Campos auxiliares (numSlots, freeSpaceOffset): necessários para manter o
 * layout "slotted page" de forma consistente e em O(1).
 *
 * Layout de pageData (4096 bytes):
 *
 *   [ registros -> ... espaço livre ... <- diretório de slots ]
 *   0                                                   4095
 *
 * - Os registros crescem do início para o fim.
 * - O diretório de slots cresce do fim para o início (4 bytes por slot).
 */
struct PageHeader {
    std::uint32_t pageId;            // Identificador da página
    std::uint16_t numRecords;        // Quantidade de registros ativos
    std::uint16_t freeSpace;         // Bytes livres (registros + slots novos)
    std::uint16_t numSlots;          // Slots alocados no diretório (ativos ou livres)
    std::uint16_t freeSpaceOffset;   // Offset onde termina o último registro
    std::uint8_t  pageData[PAGE_SIZE]; // Área de dados: registros + diretório de slots
};

/**
 * Page de 4KB que armazena registros de tamanho variável.
 *
 * O identificador de um registro é o seu slotId, que permanece estável
 * mesmo quando a Page é compactada internamente.
 */
class Page {
public:
    Page();

    /** Inicializa (ou reinicializa) a Page vazia com o pageId informado. */
    void init(std::uint32_t pageId);

    /**
     * Insere um registro.
     * @param data    Ponteiro para os bytes do registro.
     * @param size    Tamanho do registro em bytes (> 0).
     * @param slotId  [out] Slot atribuído ao registro.
     * @return true em caso de sucesso; false se os argumentos forem inválidos
     *         ou não houver espaço suficiente.
     */
    bool insertRecord(const void* data, std::uint16_t size, std::uint16_t& slotId);

    /**
     * Copia o registro do slot para outBuffer.
     * @param outSize [out] Tamanho do registro (preenchido mesmo se bufferSize
     *                for insuficiente, para o chamador poder realocar).
     * @return true em caso de sucesso; false se o slot for inválido/removido
     *         ou o buffer for pequeno demais.
     */
    bool getRecord(std::uint16_t slotId, void* outBuffer,
                   std::uint16_t bufferSize, std::uint16_t& outSize) const;

    /**
     * Substitui o conteúdo do registro (o novo tamanho pode ser diferente).
     * @return true em caso de sucesso; false se o slot for inválido, os
     *         argumentos forem inválidos ou não houver espaço para o novo
     *         tamanho. Em caso de falha, o registro original é preservado.
     */
    bool updateRecord(std::uint16_t slotId, const void* data, std::uint16_t size);

    /**
     * Remove o registro e reaproveita seu espaço.
     * @return true em caso de sucesso; false se o slot for inválido/já removido.
     */
    bool deleteRecord(std::uint16_t slotId);

    /** Bytes livres disponíveis na Page. */
    std::uint16_t getFreeSpace() const;

    std::uint32_t getPageId() const;
    std::uint16_t getNumRecords() const;

private:
    /** Entrada do diretório de slots. length == 0 indica slot livre. */
    struct Slot {
        std::uint16_t offset;
        std::uint16_t length;
    };

    static constexpr std::size_t SLOT_SIZE = sizeof(Slot);

    static std::size_t slotPosition(std::uint16_t slotId);
    Slot readSlot(std::uint16_t slotId) const;
    void writeSlot(std::uint16_t slotId, const Slot& slot);

    bool isActiveSlot(std::uint16_t slotId) const;
    std::uint16_t findFreeSlot() const;

    /** Remove os bytes do registro, compactando a área de dados. Não altera numRecords. */
    void removeRecordBytes(std::uint16_t slotId);

    /** Remove slots livres no final do diretório, devolvendo o espaço. */
    void trimTrailingFreeSlots();

    PageHeader header_;
};

#endif // PAGE
