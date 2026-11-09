#ifndef BTREE_NODE
#define BTREE_NODE

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

/**
 * Nó de uma B-Tree de grau mínimo t (t >= 2).
 *
 * Capacidades derivadas de t:
 *   - máximo de chaves  : 2t - 1
 *   - máximo de filhos  : 2t
 *
 * Responsabilidade: armazenar chaves ordenadas e ponteiros para filhos,
 * mantendo as invariantes locais do nó. Algoritmos de B-Tree (split, merge,
 * redistribuição, busca recursiva, etc.) pertencem à classe BTree.
 *
 * Propriedade de memória: o nó NÃO é dono dos filhos. Os ponteiros são
 * apenas referenciados; criar e destruir os nós filhos é responsabilidade
 * de quem usa a classe (a BTree). Por isso removeChild() devolve o ponteiro
 * e clear() apenas esquece os filhos, sem deletá-los.
 *
 * Erros de uso (índice inválido, nó cheio, chave duplicada, etc.) são
 * reportados por exceções da biblioteca padrão.
 */
class BTreeNode {
public:
    /// Tipo da chave armazenada. Precisa suportar operator<.
    using Key = int;

    /**
     * @param minDegree grau mínimo t da árvore (>= 2).
     * @param leaf      true se o nó é folha.
     * @throws std::invalid_argument se minDegree < 2.
     */
    BTreeNode(std::size_t minDegree, bool leaf);

    // ---- Consultas de estado ------------------------------------------

    bool isLeaf() const noexcept;
    bool isFull() const noexcept;   ///< keyCount() == 2t - 1
    bool isEmpty() const noexcept;  ///< keyCount() == 0
    std::size_t keyCount() const noexcept;
    std::size_t childCount() const noexcept;

    std::size_t minDegree() const noexcept;
    std::size_t maxKeys() const noexcept;
    std::size_t maxChildren() const noexcept;

    // ---- Busca ----------------------------------------------------------

    /// Índice da chave igual a `key`, ou std::nullopt se não existir.
    std::optional<std::size_t> findKey(const Key& key) const;

    /// Índice da primeira chave >= `key` (keyCount() se todas forem menores).
    /// Em nós internos, é também o índice do filho onde descer.
    std::size_t lowerBound(const Key& key) const;

    // ---- Modificação de chaves ------------------------------------------

    /**
     * Insere `key` mantendo a ordenação.
     * @return índice onde a chave foi inserida.
     * @throws std::length_error     se o nó estiver cheio.
     * @throws std::invalid_argument se a chave já existir no nó.
     */
    std::size_t insertKey(const Key& key);

    /**
     * Remove e devolve a chave na posição `index`.
     * @throws std::out_of_range se index >= keyCount().
     */
    Key removeKey(std::size_t index);

    // ---- Modificação de filhos ------------------------------------------

    /**
     * Insere `child` na posição `index` (0 <= index <= childCount()).
     * @throws std::logic_error      se o nó for folha.
     * @throws std::invalid_argument se child for nulo.
     * @throws std::out_of_range     se index > childCount().
     * @throws std::length_error     se já houver maxChildren() filhos.
     */
    void insertChild(std::size_t index, BTreeNode* child);

    /**
     * Remove o filho na posição `index` e devolve seu ponteiro
     * (o nó removido NÃO é destruído).
     * @throws std::out_of_range se index >= childCount().
     */
    BTreeNode* removeChild(std::size_t index);

    // ---- Acesso -----------------------------------------------------------

    /// @throws std::out_of_range se index >= keyCount().
    const Key& getKey(std::size_t index) const;

    /// @throws std::out_of_range se index >= childCount().
    BTreeNode* getChild(std::size_t index);
    const BTreeNode* getChild(std::size_t index) const;

    /// Remove todas as chaves e todos os filhos (sem destruir os filhos).
    void clear() noexcept;

    // ---- Validação --------------------------------------------------------

    /**
     * Verifica as invariantes locais do nó:
     *   - keyCount() <= 2t - 1;
     *   - chaves estritamente crescentes;
     *   - folha não possui filhos;
     *   - nó interno possui keyCount() + 1 filhos não nulos
     *     (ou nenhum, se também estiver sem chaves);
     *   - childCount() <= 2t.
     *
     * O número mínimo de chaves não é verificado aqui, pois depende de o
     * nó ser a raiz, informação que só a BTree possui.
     *
     * @param error se não nulo, recebe a descrição da primeira violação.
     * @return true se o nó é válido.
     */
    bool validate(std::string* error = nullptr) const;

private:
    std::size_t minDegree_;
    bool leaf_;
    std::vector<Key> keys_;
    std::vector<BTreeNode*> children_;
};

#endif // BTREE_NODE
