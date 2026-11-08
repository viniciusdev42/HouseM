#ifndef BTREE
#define BTREE

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

/**
 * Árvore B (chave -> valor) com grau mínimo t (t >= 2).
 *
 * Invariantes:
 *  - Todo nó, exceto a raiz, possui entre t-1 e 2t-1 chaves.
 *  - A raiz possui entre 1 e 2t-1 chaves (ou 0, se a árvore estiver vazia).
 *  - Um nó interno com k chaves possui exatamente k+1 filhos.
 *  - Todas as folhas estão na mesma profundidade.
 *
 * O nó da árvore é um detalhe de implementação (struct privada),
 * definido apenas em BTree.cpp.
 */
class BTree {
public:
    using Key = int;
    using Value = std::string;

    struct Entry {
        Key key;
        Value value;
    };

    explicit BTree(std::size_t minimumDegree = 3);
    ~BTree();

    BTree(const BTree&) = delete;
    BTree& operator=(const BTree&) = delete;

    // ---- Operações públicas ----

    /// Retorna o valor associado à chave, ou std::nullopt se ela não existir.
    std::optional<Value> search(Key key) const;

    /// Insere o par. Retorna false (sem alterar a árvore) se a chave já existir.
    bool insert(Key key, Value value);

    /// Remove a chave. Retorna false se ela não existir.
    bool remove(Key key);

    /// Substitui o valor de uma chave existente. Retorna false se ela não existir.
    bool update(Key key, Value value);

    /// Menor / maior entrada da árvore (std::nullopt se vazia).
    std::optional<Entry> findMin() const;
    std::optional<Entry> findMax() const;

    /// Percorre a árvore em ordem crescente de chaves.
    void traverse(const std::function<void(const Key&, const Value&)>& visit) const;

    /// Retorna, em ordem crescente, todas as entradas com lower <= chave <= upper.
    /// Lança std::invalid_argument se lower > upper.
    std::vector<Entry> rangeSearch(Key lower, Key upper) const;

    std::size_t size() const noexcept;
    bool empty() const noexcept;

private:
    struct Node;

    // ---- Operações estruturais ----

    /// Divide o filho cheio parent->children[childIndex] em dois nós,
    /// promovendo a chave mediana para o pai.
    void splitChild(Node* parent, std::size_t childIndex);

    /// Funde children[leftIndex] e children[leftIndex + 1], rebaixando
    /// a chave separadora do pai para o nó resultante.
    void mergeChildren(Node* parent, std::size_t leftIndex);

    /// Empresta uma chave do irmão esquerdo (rotação à direita via pai).
    void borrowFromLeft(Node* parent, std::size_t childIndex);

    /// Empresta uma chave do irmão direito (rotação à esquerda via pai).
    void borrowFromRight(Node* parent, std::size_t childIndex);

    /// Substitui a chave interna parent->entries[keyIndex] pelo predecessor
    /// ou sucessor (removendo-o da subárvore de origem). Retorna false se
    /// nenhum dos dois filhos adjacentes pode ceder uma chave.
    bool promoteKey(Node* node, std::size_t keyIndex);

    /// Se a raiz ficou sem chaves e não é folha, seu único filho vira a raiz
    /// (a altura da árvore diminui em 1).
    void collapseRoot();

    /// Garante que parent->children[childIndex] tenha ao menos t chaves,
    /// emprestando ou fundindo. Retorna o índice do filho em que a
    /// descida deve continuar (pode mudar após uma fusão com o irmão esquerdo).
    std::size_t rebalance(Node* parent, std::size_t childIndex);

    // ---- Auxiliares ----

    void insertNonFull(Node* node, Entry entry);
    bool removeFromNode(Node* node, Key key);
    bool removeFromInternal(Node* node, std::size_t keyIndex);

    Entry* findEntry(Key key) const;
    bool collectRange(const Node* node, Key lower, Key upper,
                      std::vector<Entry>& out) const;
    void traverseNode(const Node* node,
                      const std::function<void(const Key&, const Value&)>& visit) const;

    static std::size_t lowerBound(const Node* node, Key key);
    static const Entry& leftmostEntry(const Node* node);
    static const Entry& rightmostEntry(const Node* node);

    std::size_t minKeys() const noexcept { return minimumDegree_ - 1; }
    std::size_t maxKeys() const noexcept { return 2 * minimumDegree_ - 1; }

    std::size_t minimumDegree_;
    std::size_t size_;
    std::unique_ptr<Node> root_;
};

#endif // BTREE
