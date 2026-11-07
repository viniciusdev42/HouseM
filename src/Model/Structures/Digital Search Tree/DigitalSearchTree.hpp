#ifndef DIGITAL_SEARCH_TREE
#define DIGITAL_SEARCH_TREE

#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <stdexcept>
#include <type_traits>

namespace DigitalSearchTree {

/**
 * Nó da Digital Search Tree.
 *
 * Diferente de uma trie, cada nó armazena uma chave completa. O bit da chave
 * inspecionado na profundidade d (do mais significativo para o menos
 * significativo) decide se a busca segue para o filho esquerdo (0) ou direito (1).
 */
template <typename KeyType>
struct DigitalNode {
    KeyType key;
    std::unique_ptr<DigitalNode> left;
    std::unique_ptr<DigitalNode> right;

    explicit DigitalNode(const KeyType& k) : key(k), left(nullptr), right(nullptr) {}

    bool IsLeaf() const noexcept { return !left && !right; }
};

/** Ordem de visita usada por DigitalSearchTree::Traverse. */
enum class TraversalOrder { PreOrder, InOrder, PostOrder };

/**
 * Digital Search Tree (DST) para chaves inteiras sem sinal.
 *
 * Os bits são extraídos a partir do bit mais significativo (MSB).
 * Chaves duplicadas não são permitidas.
 *
 * Complexidade de Insert/Search/Remove/Contains: O(b), onde b = número de bits
 * da chave (ex.: 32 para uint32_t). Isso também limita a altura da árvore a b.
 */
template <typename KeyType = std::uint32_t>
class DigitalSearchTree {
    static_assert(std::is_integral<KeyType>::value &&
                      std::is_unsigned<KeyType>::value &&
                      !std::is_same<KeyType, bool>::value,
                  "DigitalSearchTree requer um tipo inteiro sem sinal (ex.: uint32_t).");

public:
    using Node = DigitalNode<KeyType>;
    using Visitor = std::function<void(const KeyType&)>;

    static constexpr std::size_t kKeyBits =
        static_cast<std::size_t>(std::numeric_limits<KeyType>::digits);

    DigitalSearchTree() = default;
    ~DigitalSearchTree() = default;

    // A árvore é dona exclusiva dos nós (unique_ptr): cópia é proibida, movimentação é permitida.
    DigitalSearchTree(const DigitalSearchTree&) = delete;
    DigitalSearchTree& operator=(const DigitalSearchTree&) = delete;
    DigitalSearchTree(DigitalSearchTree&& other) noexcept
        : root_(std::move(other.root_)), size_(other.size_) {
        other.size_ = 0;
    }
    DigitalSearchTree& operator=(DigitalSearchTree&& other) noexcept {
        if (this != &other) {
            root_ = std::move(other.root_);
            size_ = other.size_;
            other.size_ = 0;
        }
        return *this;
    }

    /**
     * Retorna o bit da chave na posição 'depth', contando a partir do bit mais
     * significativo (depth = 0 é o MSB; depth = kKeyBits - 1 é o LSB).
     * @throws std::out_of_range se depth >= kKeyBits.
     */
    static bool GetBit(const KeyType& key, std::size_t depth) {
        if (depth >= kKeyBits) {
            throw std::out_of_range("DigitalSearchTree::GetBit: profundidade fora do intervalo de bits da chave.");
        }
        return ((key >> (kKeyBits - 1 - depth)) & static_cast<KeyType>(1)) != 0;
    }

    /**
     * Insere a chave na primeira posição livre do caminho definido por seus bits.
     * @return true se inseriu; false se a chave já existia.
     */
    bool Insert(const KeyType& key) {
        std::unique_ptr<Node>* slot = &root_;
        for (std::size_t depth = 0; *slot != nullptr; ++depth) {
            if ((*slot)->key == key) {
                return false;
            }
            // Chaves distintas divergem em algum bit antes de kKeyBits, então
            // 'depth' nunca ultrapassa o limite enquanto houver nó ocupado.
            slot = &ChildSlot(**slot, GetBit(key, depth));
        }
        *slot = std::make_unique<Node>(key);
        ++size_;
        return true;
    }

    /** @return ponteiro para o nó que contém a chave, ou nullptr se não existir. */
    const Node* Search(const KeyType& key) const {
        // const_cast é seguro: FindSlot apenas navega, sem modificar a árvore.
        const std::unique_ptr<Node>* slot =
            const_cast<DigitalSearchTree*>(this)->FindSlot(key);
        return slot != nullptr ? slot->get() : nullptr;
    }

    /** @return true se a chave estiver presente na árvore. */
    bool Contains(const KeyType& key) const { return Search(key) != nullptr; }

    /**
     * Remove a chave da árvore.
     *
     * Se o nó for folha, ele é simplesmente removido. Caso contrário, a chave
     * de uma folha descendente assume o seu lugar: como toda chave na subárvore
     * compartilha o prefixo de bits do caminho até o nó, a invariante da DST
     * é preservada.
     *
     * @return true se removeu; false se a chave não existia.
     */
    bool Remove(const KeyType& key) {
        std::unique_ptr<Node>* slot = FindSlot(key);
        if (slot == nullptr) {
            return false;
        }

        Node& target = **slot;
        if (target.IsLeaf()) {
            slot->reset();
        } else {
            std::unique_ptr<Node>* leafSlot = &FirstExistingChildSlot(target);
            while (!(*leafSlot)->IsLeaf()) {
                leafSlot = &FirstExistingChildSlot(**leafSlot);
            }
            target.key = (*leafSlot)->key;
            leafSlot->reset();
        }
        --size_;
        return true;
    }

    /** Remove todos os nós. A altura é limitada a kKeyBits, logo a destruição recursiva é segura. */
    void Clear() noexcept {
        root_.reset();
        size_ = 0;
    }

    /**
     * Percorre a árvore chamando 'visitor' para cada chave.
     * Observação: in-order NÃO produz chaves ordenadas em uma DST.
     * @throws std::invalid_argument se visitor for vazio.
     */
    void Traverse(const Visitor& visitor,
                  TraversalOrder order = TraversalOrder::PreOrder) const {
        if (!visitor) {
            throw std::invalid_argument("DigitalSearchTree::Traverse: visitor invalido.");
        }
        TraverseNode(root_.get(), visitor, order);
    }

    std::size_t Size() const noexcept { return size_; }
    bool Empty() const noexcept { return size_ == 0; }

private:
    std::unique_ptr<Node> root_;
    std::size_t size_ = 0;

    static std::unique_ptr<Node>& ChildSlot(Node& node, bool bit) noexcept {
        return bit ? node.right : node.left;
    }

    static std::unique_ptr<Node>& FirstExistingChildSlot(Node& node) noexcept {
        return node.left ? node.left : node.right;
    }

    /** @return o slot (ponteiro de posse) que contém a chave, ou nullptr. */
    std::unique_ptr<Node>* FindSlot(const KeyType& key) {
        std::unique_ptr<Node>* slot = &root_;
        for (std::size_t depth = 0; *slot != nullptr; ++depth) {
            if ((*slot)->key == key) {
                return slot;
            }
            slot = &ChildSlot(**slot, GetBit(key, depth));
        }
        return nullptr;
    }

    static void TraverseNode(const Node* node, const Visitor& visitor, TraversalOrder order) {
        if (node == nullptr) {
            return;
        }
        if (order == TraversalOrder::PreOrder) {
            visitor(node->key);
        }
        TraverseNode(node->left.get(), visitor, order);
        if (order == TraversalOrder::InOrder) {
            visitor(node->key);
        }
        TraverseNode(node->right.get(), visitor, order);
        if (order == TraversalOrder::PostOrder) {
            visitor(node->key);
        }
    }
};

}  // namespace DigitalSearchTree

#endif  // DIGITAL_SEARCH_TREE
