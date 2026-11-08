#ifndef BTREE_NODE
#define BTREE_NODE

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

/**
 * Nó de uma Árvore B (B-Tree) de grau mínimo t (convenção CLRS).
 *
 * Invariantes de capacidade:
 *   - número máximo de chaves  : 2t - 1
 *   - número máximo de filhos  : 2t
 *   - nó interno consistente   : filhos == chaves + 1
 *   - nó folha                 : nenhum filho
 *
 * O nó é o dono exclusivo de seus filhos (std::unique_ptr), portanto é
 * move-only. A regra de mínimo de chaves (t - 1) depende de o nó ser ou não
 * a raiz, informação que pertence à árvore e não ao nó; por isso validate()
 * não a verifica.
 *
 * Nota de organização: por ser um template, a implementação (BTreeNode.cpp)
 * é incluída ao final deste header. Não é necessário compilar o .cpp
 * separadamente (fazê-lo é inofensivo, pois ambos possuem include guards).
 */
template <typename Key, typename Compare = std::less<Key>>
class BTreeNode {
public:
    using key_type = Key;
    using size_type = std::size_t;
    using NodePtr = std::unique_ptr<BTreeNode>;

    /**
     * @param minDegree grau mínimo t (>= 2).
     * @param leaf      indica se o nó é folha.
     * @throws std::invalid_argument se minDegree < 2.
     */
    explicit BTreeNode(size_type minDegree, bool leaf = true,
                       Compare comp = Compare());

    BTreeNode(const BTreeNode&) = delete;
    BTreeNode& operator=(const BTreeNode&) = delete;
    BTreeNode(BTreeNode&&) noexcept = default;
    BTreeNode& operator=(BTreeNode&&) noexcept = default;
    ~BTreeNode() = default;

    // ----- Consultas de estado -----
    bool isLeaf() const noexcept;
    bool isFull() const noexcept;   // keyCount() == 2t - 1
    bool isEmpty() const noexcept;  // keyCount() == 0
    size_type keyCount() const noexcept;
    size_type childCount() const noexcept;

    // ----- Chaves -----

    /**
     * Retorna o índice da primeira chave que NÃO é menor que `key`
     * (lower bound), ou keyCount() se todas forem menores.
     * Se `found` não for nulo, recebe true quando a chave naquele índice
     * é equivalente a `key`. Em um nó interno, o índice retornado também é
     * o do filho para onde a busca deve descer quando a chave não existe.
     */
    size_type findKey(const Key& key, bool* found = nullptr) const;

    /**
     * Insere a chave mantendo a ordenação e retorna o índice de inserção.
     * @throws std::length_error     se o nó estiver cheio.
     * @throws std::invalid_argument se a chave já existir no nó.
     */
    size_type insertKey(Key key);

    /**
     * Remove e retorna a chave na posição `index`.
     * @throws std::out_of_range se index >= keyCount().
     */
    Key removeKey(size_type index);

    /** @throws std::out_of_range se index >= keyCount(). */
    const Key& getKey(size_type index) const;

    // ----- Filhos -----

    /**
     * Insere o filho na posição `index` (0 <= index <= childCount()).
     * @throws std::logic_error      se o nó for folha.
     * @throws std::invalid_argument se child for nulo ou tiver grau mínimo
     *                               diferente do deste nó.
     * @throws std::length_error     se o nó já tiver 2t filhos.
     * @throws std::out_of_range     se index > childCount().
     */
    void insertChild(size_type index, NodePtr child);

    /**
     * Remove o filho na posição `index` e transfere sua posse ao chamador.
     * @throws std::out_of_range se index >= childCount().
     */
    NodePtr removeChild(size_type index);

    /** @throws std::out_of_range se index >= childCount(). */
    BTreeNode* getChild(size_type index);
    const BTreeNode* getChild(size_type index) const;

    // ----- Manutenção -----

    /** Remove todas as chaves e destrói todos os filhos (o flag de folha é mantido). */
    void clear() noexcept;

    /**
     * Verifica a consistência do nó e de toda a sua subárvore:
     *   - limite máximo de chaves;
     *   - chaves estritamente ordenadas;
     *   - folha sem filhos; interno com filhos == chaves + 1
     *     (ou totalmente vazio);
     *   - filhos não nulos, com mesmo grau mínimo;
     *   - chaves de cada filho dentro dos separadores do pai;
     *   - todas as folhas na mesma profundidade.
     *
     * @param error se não nulo, recebe a descrição da primeira violação.
     * @return true se tudo estiver consistente.
     */
    bool validate(std::string* error = nullptr) const;

private:
    bool validateSubtree(const Key* lowerBound, const Key* upperBound,
                         size_type depth, long& leafDepth,
                         std::string* error) const;

    size_type maxKeys() const noexcept;
    size_type maxChildren() const noexcept;

    size_type minDegree_;
    bool leaf_;
    Compare comp_;
    std::vector<Key> keys_;
    std::vector<NodePtr> children_;
};

#endif  // BTREE_NODE
