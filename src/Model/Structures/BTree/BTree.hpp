#ifndef BTREE
#define BTREE

#include <cstddef>
#include <functional>
#include <optional>
#include <vector>

#include "BTreeNode.hpp"

/**
 * B-Tree de grau mínimo t (t >= 2) que armazena chaves únicas.
 *
 * Invariantes (CLRS):
 *   - todo nó, exceto a raiz, possui entre t-1 e 2t-1 chaves;
 *   - a raiz possui entre 1 e 2t-1 chaves (ou a árvore está vazia);
 *   - todas as folhas estão na mesma profundidade.
 *
 * Estratégia: inserção e remoção em passada única (descendo pela árvore),
 * dividindo nós cheios na inserção e garantindo ao menos t chaves nos nós
 * visitados na remoção. Não é necessário subir de volta para rebalancear.
 *
 * Propriedade de memória: a BTree é dona de todos os nós (BTreeNode não é).
 * A classe é movível, mas não copiável.
 */
class BTree {
public:
    using Key = BTreeNode::Key;

    /// @throws std::invalid_argument se minDegree < 2.
    explicit BTree(std::size_t minDegree = 2);
    ~BTree();

    BTree(const BTree&) = delete;
    BTree& operator=(const BTree&) = delete;
    BTree(BTree&& other) noexcept;
    BTree& operator=(BTree&& other) noexcept;

    // ---- Operações principais ------------------------------------------

    /// Insere a chave. @return false se ela já existia (árvore inalterada).
    bool insert(const Key& key);

    /// Remove a chave. @return false se ela não existia (árvore inalterada).
    bool remove(const Key& key);

    bool contains(const Key& key) const;

    /// Destrói todos os nós; a árvore passa a ficar vazia.
    void clear() noexcept;

    bool empty() const noexcept;

    /// Número de chaves armazenadas.
    std::size_t size() const noexcept;

    /// Número de níveis da árvore: 0 se vazia, 1 se só a raiz (folha).
    std::size_t height() const noexcept;

    std::size_t minDegree() const noexcept;

    // ---- Consultas ordenadas ---------------------------------------------

    /// Menor chave, ou std::nullopt se a árvore estiver vazia.
    std::optional<Key> findMin() const;

    /// Maior chave, ou std::nullopt se a árvore estiver vazia.
    std::optional<Key> findMax() const;

    /// Menor chave >= `key`, ou std::nullopt se não existir.
    std::optional<Key> lowerBound(const Key& key) const;

    /// Menor chave > `key`, ou std::nullopt se não existir.
    std::optional<Key> upperBound(const Key& key) const;

    /// Todas as chaves em [low, high], em ordem crescente.
    /// Devolve vetor vazio se low > high.
    std::vector<Key> rangeSearch(const Key& low, const Key& high) const;

    /// Visita todas as chaves em ordem crescente.
    void traverseInOrder(const std::function<void(const Key&)>& visit) const;

private:
    // ---- Inserção ----------------------------------------------------------

    /// Divide o filho cheio `parent->getChild(index)`; a chave mediana sobe
    /// para `parent`, que não pode estar cheio.
    void splitChild(BTreeNode* parent, std::size_t index);

    /// Insere em uma subárvore cuja raiz não está cheia.
    void insertNonFull(BTreeNode* node, const Key& key);

    // ---- Remoção -----------------------------------------------------------

    /// Remove `key` (que deve existir) da subárvore de `node`.
    /// Pré-condição: `node` é a raiz ou possui >= t chaves.
    void removeFromNode(BTreeNode* node, const Key& key);

    /// Remove a chave de índice `index` de um nó interno.
    void removeFromInternalNode(BTreeNode* node, std::size_t index);

    /// Une os filhos `index` e `index + 1` (mais a chave separadora) num só.
    void mergeChildren(BTreeNode* parent, std::size_t index);

    /// Move uma chave do irmão esquerdo, via pai, para o filho `index`.
    void borrowFromPrevious(BTreeNode* parent, std::size_t index);

    /// Move uma chave do irmão direito, via pai, para o filho `index`.
    void borrowFromNext(BTreeNode* parent, std::size_t index);

    /// Garante que o filho `index` tenha >= t chaves (empresta ou funde).
    /// @return índice atual do filho que contém a subárvore original
    ///         (muda para index - 1 se houve fusão com o irmão esquerdo).
    std::size_t fillChild(BTreeNode* parent, std::size_t index);

    /// Maior chave da subárvore à esquerda de `node->getKey(index)`.
    Key getPredecessor(const BTreeNode* node, std::size_t index) const;

    /// Menor chave da subárvore à direita de `node->getKey(index)`.
    Key getSuccessor(const BTreeNode* node, std::size_t index) const;

    /// Se a raiz ficou sem chaves, a substitui pelo único filho (ou a
    /// destrói, se for folha).
    void collapseRoot();

    // ---- Auxiliares ----------------------------------------------------------

    static void destroySubtree(BTreeNode* node) noexcept;
    static void traverseNode(const BTreeNode* node,
                             const std::function<void(const Key&)>& visit);
    static void rangeSearchNode(const BTreeNode* node, const Key& low,
                                const Key& high, std::vector<Key>& out);

    std::size_t minDegree_;
    BTreeNode* root_ = nullptr;
    std::size_t size_ = 0;
};

#endif // BTREE
