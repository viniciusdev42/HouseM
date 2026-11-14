#ifndef PRIORITY_LIST
#define PRIORITY_LIST

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * Elemento armazenado na lista de prioridades.
 * O `id` identifica o elemento de forma única e permite localizá-lo em O(1)
 * para atualização de prioridade e remoção.
 */
struct Element {
    int id;
    std::string name;
    int priority;
};

/**
 * Lista de Prioridades implementada sobre um Heap Binário (max-heap):
 * o elemento com MAIOR valor de `priority` está sempre no topo.
 *
 * Complexidades:
 *   - construção a partir de coleção ... O(n)
 *   - insert ........................... O(log n)
 *   - peek ............................. O(1)
 *   - extractTop ....................... O(log n)
 *   - updatePriority ................... O(log n)
 *   - remove ........................... O(log n)
 *
 * Erros:
 *   - std::invalid_argument: id duplicado ou inexistente.
 *   - std::out_of_range: consulta/extração em lista vazia.
 */
class PriorityList {
public:
    PriorityList() = default;

    /** Constrói o heap a partir de uma coleção (heapify bottom-up, O(n)). */
    explicit PriorityList(std::vector<Element> elements);

    /** Insere um novo elemento. Lança std::invalid_argument se o id já existir. */
    void insert(const Element& element);

    /** Consulta o elemento de maior prioridade sem removê-lo. */
    const Element& peek() const;

    /** Remove e retorna o elemento de maior prioridade. */
    Element extractTop();

    /** Altera a prioridade do elemento `id` e reorganiza o heap. */
    void updatePriority(int id, int newPriority);

    /** Remove o elemento `id` (em qualquer posição) e o retorna. */
    Element remove(int id);

    bool contains(int id) const;
    std::size_t size() const noexcept;
    bool empty() const noexcept;

private:
    std::vector<Element> heap_;
    // Mapa id -> posição no vetor; evita busca linear O(n) em update/remove.
    std::unordered_map<int, std::size_t> positionById_;

    static std::size_t parentOf(std::size_t index) noexcept;
    static std::size_t leftChildOf(std::size_t index) noexcept;

    bool hasHigherPriority(std::size_t a, std::size_t b) const;
    void swapNodes(std::size_t a, std::size_t b);

    std::size_t siftUp(std::size_t index);
    void siftDown(std::size_t index);
    void restoreHeapProperty(std::size_t index);

    std::size_t findPosition(int id) const;
    Element removeAt(std::size_t index);
};

#endif // PRIORITY_LIST
